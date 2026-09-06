#include "core/anims/AirDocument.h"
#include "core/engine/Engine.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/script/LuaApi.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"

#include <QFile>
#include <QFileInfo>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QUndoStack>

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace OFS::Script {

namespace {

// Lua handles to parts of a file. They look the part up again on each use.
struct ParentRef
{
    QPointer<Project> project;
};
struct CursorRef
{
    QPointer<QTextDocument> document;
    QTextCursor cursor;
};
struct SpritesRef
{
    QPointer<SpriteFile> file;
};
struct SpriteRef
{
    QPointer<SpriteFile> file;
    int index = 0;
};
struct PalettesRef
{
    QPointer<SpriteFile> file;
};
struct PaletteRef
{
    QPointer<SpriteFile> file;
    int index = 0;
};
struct ImageInfo
{
    int format = 0; // 8: indexed, 32: true color
    int width = 0;
    int height = 0;
};
struct AnimationsRef
{
    QPointer<CodeFile> file;
};
struct AnimationRef
{
    QPointer<CodeFile> file;
    int index = 0;
};
struct FrameRef
{
    QPointer<CodeFile> file;
    int animation = 0;
    int frame = 0;
};
struct BlendInfo
{
    int mode = 0;
    double source = 1;
    double dest = 1;
};
struct BoxesRef
{
    QPointer<CodeFile> file;
    int animation = 0;
    int frame = 0;
    int type = 1;
};
struct BoxRef
{
    QPointer<CodeFile> file;
    int animation = 0;
    int frame = 0;
    int type = 1;
    int index = 0;
};
struct SoundsRef
{
    QPointer<SoundFile> file;
};
struct SoundRef
{
    QPointer<SoundFile> file;
    int index = 0;
};
// FF classes that FFS lists but no function returns.
template <int N>
struct Unreachable
{};

[[noreturn]] void fail(const char* message)
{
    throw std::runtime_error(message);
}

QString editorOfType(int type)
{
    switch (type) {
    case 0:
        return QStringLiteral("definitions");
    case 1:
        return QStringLiteral("sprites");
    case 2:
        return QStringLiteral("sounds");
    case 3:
        return QStringLiteral("animations");
    case 4:
        return QStringLiteral("states");
    default:
        return QString();
    }
}

ProjectFile* loadFile(Project& project, const QString& editorId, const QString& path)
{
    if (ProjectFile* open = project.file(path))
        return open;
    QString error;
    if (editorId == QLatin1String("sprites"))
        return project.spriteFile(path, editorId, &error);
    if (editorId == QLatin1String("sounds"))
        return project.soundFile(path, editorId, &error);
    return project.codeFile(path, editorId, &error);
}

QList<ProjectFile*> filesOf(Project& project, const QString& editorId)
{
    QList<ProjectFile*> files;
    for (const QString& path : project.editorFiles(editorId)) {
        if (ProjectFile* file = loadFile(project, editorId, path); file && !files.contains(file))
            files << file;
    }
    return files;
}

template <typename T>
sol::table listOf(sol::this_state state, const QList<T>& items)
{
    sol::state_view view(state);
    sol::table table = view.create_table(int(items.size()), 0);
    for (int i = 0; i < items.size(); ++i)
        table[i + 1] = items[i];
    return table;
}

QUndoStack* undoStackOf(ProjectFile* file)
{
    if (auto* sprites = qobject_cast<SpriteFile*>(file))
        return sprites->undoStack();
    if (auto* sounds = qobject_cast<SoundFile*>(file))
        return sounds->undoStack();
    return nullptr;
}

// --- sprites

const SpriteEntry* spriteOf(const SpriteRef& ref)
{
    if (!ref.file || ref.index < 0 || ref.index >= ref.file->document().spriteCount())
        return nullptr;
    return &ref.file->document().sprites()[ref.index];
}

const SpritePalette* paletteOf(const PaletteRef& ref)
{
    if (!ref.file || ref.index < 0 || ref.index >= ref.file->document().palettes().size())
        return nullptr;
    return ref.file->document().palettes()[ref.index].get();
}

void editSprite(SpriteRef& ref, const char* what, const std::function<void(SpriteEntry&)>& change)
{
    if (!spriteOf(ref))
        fail("the sprite no longer exists");
    SpriteDocument after = ref.file->document();
    change(after.sprites()[ref.index]);
    ref.file->apply(QString::fromLatin1(what), after);
}

// --- animations

AirDocument airOf(const CodeFile* file)
{
    return file ? AirDocument::parse(file->document()->toPlainText()) : AirDocument();
}

const AirAnimation* animationOf(const AirDocument& air, int index)
{
    return index >= 0 && index < air.count() ? &air.animations()[index] : nullptr;
}

const AirFrame* frameOf(const AirDocument& air, int animation, int frame)
{
    const AirAnimation* a = animationOf(air, animation);
    return a && frame >= 0 && frame < a->frames.size() ? &a->frames[frame] : nullptr;
}

QString lineText(const CodeFile* file, int line)
{
    if (!file || line < 0)
        return QString();
    return file->document()->findBlockByNumber(line).text();
}

QString linesText(const CodeFile* file, int first, int last)
{
    QStringList lines;
    for (int line = first; line <= last && line >= 0; ++line)
        lines << lineText(file, line);
    return lines.join(QLatin1Char('\n'));
}

int blendMode(const AirTransparency& t)
{
    switch (t.mode) {
    case AirTransparency::Mode::Addition:
    case AirTransparency::Mode::Addition50:
        return 1;
    case AirTransparency::Mode::Subtraction:
        return 2;
    default:
        return 0;
    }
}

QString flipText(const AirFrame& frame)
{
    return QString(frame.flipV ? QStringLiteral("V") : QString()) + (frame.flipH ? QStringLiteral("H") : QString());
}

// PropertyItem: rows of an editor's property panel with readable values.
struct Properties
{
    QStringList names;
    QList<sol::object> values;
};

Properties animationProperties(sol::this_state state, const AnimationRef& ref)
{
    const AirDocument air = airOf(ref.file);
    const AirAnimation* a = animationOf(air, ref.index);
    if (!a)
        fail("the animation no longer exists");
    sol::state_view view(state);
    return {{QStringLiteral("DisplayName"), QStringLiteral("Name"), QStringLiteral("Identifier")},
            {sol::make_object(view, toStd(a->caption())), sol::make_object(view, toStd(a->name)),
             sol::make_object(view, a->number)}};
}

Properties frameProperties(sol::this_state state, const FrameRef& ref)
{
    const AirDocument air = airOf(ref.file);
    const AirFrame* f = frameOf(air, ref.animation, ref.frame);
    if (!f)
        fail("the frame no longer exists");
    sol::state_view view(state);
    auto object = [&view](auto value) {
        return sol::make_object(view, value);
    };
    return {{QStringLiteral("Identifier"), QStringLiteral("Offset"), QStringLiteral("Delay"), QStringLiteral("Scale"),
             QStringLiteral("Angle"), QStringLiteral("Flip"), QStringLiteral("LoopPoint"), QStringLiteral("BlendMode"),
             QStringLiteral("InterpolateBlend"), QStringLiteral("InterpolateOffset"),
             QStringLiteral("InterpolateScale"), QStringLiteral("InterpolateAngle")},
            {object(toStd(QStringLiteral("%1,%2").arg(f->group).arg(f->number))),
             object(toStd(QStringLiteral("%1,%2").arg(f->x).arg(f->y))), object(f->time),
             object(toStd(QStringLiteral("%1,%2").arg(f->xScale).arg(f->yScale))), object(f->angle),
             object(toStd(flipText(*f))), object(f->loopStart), object(toStd(f->transparency.text())),
             object((f->interpolate & AirFrame::InterpolateBlend) != 0),
             object((f->interpolate & AirFrame::InterpolateOffset) != 0),
             object((f->interpolate & AirFrame::InterpolateScale) != 0),
             object((f->interpolate & AirFrame::InterpolateAngle) != 0)}};
}

Properties paletteProperties(sol::this_state state, const PaletteRef& ref)
{
    const SpritePalette* p = paletteOf(ref);
    if (!p)
        fail("the palette no longer exists");
    sol::state_view view(state);
    return {{QStringLiteral("Identifier"), QStringLiteral("SubItem"), QStringLiteral("ColorCount")},
            {sol::make_object(view, int(p->palette.group)), sol::make_object(view, int(p->palette.number)),
             sol::make_object(view, int(p->palette.colors.size()))}};
}

template <typename Ref>
auto propertyMethods(Properties (*get)(sol::this_state, const Ref&))
{
    return std::make_tuple([get](Ref& ref, sol::this_state state) { return int(get(state, ref).names.size()); },
                           [get](Ref& ref, int i, sol::this_state state) -> std::string {
                               const Properties p = get(state, ref);
                               return i >= 0 && i < p.names.size() ? toStd(p.names[i]) : std::string();
                           },
                           [get](Ref& ref, int i, sol::this_state state) -> sol::object {
                               const Properties p = get(state, ref);
                               return i >= 0 && i < p.values.size()
                                          ? p.values[i]
                                          : sol::make_object(sol::state_view(state), sol::lua_nil);
                           });
}

} // namespace

void registerProjectApi(sol::state& lua, sol::table api, Session& session)
{
    Q_UNUSED(lua)
    Q_UNUSED(session)

    api.new_usertype<Engine>(
        "Engine", sol::no_constructor,                                                  //
        "name", sol::property([](Engine& e) { return toStd(e.name()); }),               //
        "displayName", sol::property([](Engine& e) { return toStd(e.displayName()); }), //
        "path", sol::property([](Engine& e) { return toStd(e.dataDir()); }),            //
        "singleFileFilters",
        [](Engine& e, sol::this_state state) {
            sol::state_view view(state);
            sol::table table = view.create_table();
            const QMap<int, QString> filters = e.scriptFileFilters();
            for (auto it = filters.cbegin(); it != filters.cend(); ++it)
                table[it.key()] = toStd(it.value());
            return table;
        },
        "createFile", [](Engine&) { fail("Engine:createFile is not available"); });

    api.new_usertype<Project>(
        "Project", sol::no_constructor,                                                              //
        "name", sol::property([](Project& p) { return toStd(p.name()); }),                           //
        "fileName", sol::property([](Project& p) { return toStd(p.filePath()); }),                   //
        "path", sol::property([](Project& p) { return toStd(p.directory()); }),                      //
        "type", sol::property([](Project& p) { return p.engine()->scriptProjectType(p.typeId()); }), //
        "version", sol::property([](Project& p) { return toStd(p.versionName()); }),                 //
        "engine", sol::property([](Project& p) { return p.engine(); }),                              //
        "isModified", sol::property([](Project& p) { return p.isModified(); }),                      //
        "isEmpty", sol::property([](Project& p) {
            const QStringList editors = p.editorIds();
            return std::all_of(editors.cbegin(), editors.cend(),
                               [&p](const QString& id) { return p.editorFiles(id).isEmpty(); });
        }),
        "archive", sol::property([](Project& p) {
            ScriptArchive archive;
            if (!p.archivePath().isEmpty()) {
                archive.root = p.archivePath();
                archive.zip = true;
                archive.readOnly = true;
            } else {
                archive.root = p.directory();
            }
            return archive;
        }),
        "parent", sol::property([](Project& p) { return ParentRef{&p}; }), //
        "files",
        [](Project& p, sol::this_state state) {
            QList<ProjectFile*> files;
            for (const QString& editorId : p.editorIds()) {
                for (ProjectFile* file : filesOf(p, editorId)) {
                    if (!files.contains(file))
                        files << file;
                }
            }
            return listOf(state, files);
        },
        "filesByType",
        [](Project& p, int type, sol::this_state state) { return listOf(state, filesOf(p, editorOfType(type))); },
        "fileByType",
        [](Project& p, int type) -> ProjectFile* {
            const QList<ProjectFile*> files = filesOf(p, editorOfType(type));
            return files.isEmpty() ? nullptr : files.first();
        },
        "saveAll", [](Project& p) { return p.save(); });

    api.new_usertype<ParentRef>(
        "ParentProject", sol::no_constructor, //
        "engine", sol::property([](ParentRef& r) -> Engine* { return r.project ? r.project->engine() : nullptr; }),
        "archive", sol::property([](ParentRef& r) {
            ScriptArchive archive;
            archive.root = r.project ? r.project->directory() : QString();
            return archive;
        }),
        "save", [](ParentRef& r) { return r.project && r.project->save(); }, //
        "saveAs", [](ParentRef&) { fail("ParentProject:saveAs is not available"); }, "backup",
        [](ParentRef&) { fail("ParentProject:backup is not available"); }, "restoreBackup",
        [](ParentRef&) { fail("ParentProject:restoreBackup is not available"); }, "openedProjects",
        [](ParentRef& r, sol::this_state state) {
            return listOf(state, r.project ? QList<Project*>{r.project.data()} : QList<Project*>());
        });

    api.new_usertype<ProjectFile>(
        "ProjectFile", sol::no_constructor,                                                      //
        "fileName", sol::property([](ProjectFile& f) { return toStd(f.path()); }),               //
        "fileExists", sol::property([](ProjectFile& f) { return QFileInfo::exists(f.path()); }), //
        "isModified", sol::property([](ProjectFile& f) { return f.isModified(); }),              //
        "isReadOnly",
        sol::property([](ProjectFile& f) { return QFileInfo::exists(f.path()) && !QFileInfo(f.path()).isWritable(); }),
        "isEmpty", sol::property([](ProjectFile& f) {
            if (auto* code = qobject_cast<CodeFile*>(&f))
                return code->document()->isEmpty();
            if (auto* sprites = qobject_cast<SpriteFile*>(&f))
                return sprites->document().spriteCount() == 0;
            if (auto* sounds = qobject_cast<SoundFile*>(&f))
                return sounds->document().count() == 0;
            return true;
        }),
        "project", sol::property([](ProjectFile& f) { return qobject_cast<Project*>(f.parent()); }), //
        "archive", sol::property([](ProjectFile& f) {
            ScriptArchive archive;
            archive.root = QFileInfo(f.path()).absolutePath();
            return archive;
        }),
        "asSourceCode", [](ProjectFile& f) { return qobject_cast<CodeFile*>(&f); }, //
        "asSprites",
        [](ProjectFile& f) -> sol::optional<SpritesRef> {
            if (auto* sprites = qobject_cast<SpriteFile*>(&f))
                return SpritesRef{sprites};
            return sol::nullopt;
        },
        "asAnimations",
        [](ProjectFile& f) -> sol::optional<AnimationsRef> {
            auto* code = qobject_cast<CodeFile*>(&f);
            if (code && f.editorId() == QLatin1String("animations"))
                return AnimationsRef{code};
            return sol::nullopt;
        },
        "asSounds",
        [](ProjectFile& f) -> sol::optional<SoundsRef> {
            if (auto* sounds = qobject_cast<SoundFile*>(&f))
                return SoundsRef{sounds};
            return sol::nullopt;
        },
        // One undo step for the changes in between.
        "beginChange",
        [](ProjectFile& f) {
            if (auto* code = qobject_cast<CodeFile*>(&f))
                QTextCursor(code->document()).beginEditBlock();
            else if (QUndoStack* stack = undoStackOf(&f))
                stack->beginMacro(QStringLiteral("Script"));
        },
        "endChange",
        [](ProjectFile& f) {
            if (auto* code = qobject_cast<CodeFile*>(&f))
                QTextCursor(code->document()).endEditBlock();
            else if (QUndoStack* stack = undoStackOf(&f))
                stack->endMacro();
        },
        "undo",
        [](ProjectFile& f) {
            if (auto* code = qobject_cast<CodeFile*>(&f))
                code->document()->undo();
            else if (QUndoStack* stack = undoStackOf(&f))
                stack->undo();
        },
        "redo",
        [](ProjectFile& f) {
            if (auto* code = qobject_cast<CodeFile*>(&f))
                code->document()->redo();
            else if (QUndoStack* stack = undoStackOf(&f))
                stack->redo();
        },
        "reset", [](ProjectFile& f) { return f.load(nullptr); }, //
        "toFile", [](ProjectFile& f, const std::string& path) { return f.writeCopy(fromStd(path), nullptr); },
        "fromFile",
        [](ProjectFile& f, const std::string& path) {
            auto* code = qobject_cast<CodeFile*>(&f);
            if (!code)
                fail("ProjectFile:fromFile reads text files only");
            QFile file(fromStd(path));
            if (!file.open(QIODevice::ReadOnly))
                return false;
            QString encoding;
            QByteArray bom;
            QTextCursor cursor(code->document());
            cursor.select(QTextCursor::Document);
            cursor.insertText(CodeFile::decode(file.readAll(), &encoding, &bom));
            return true;
        });

    api.new_usertype<CodeFile>(
        "CodeDocument", sol::no_constructor, sol::base_classes, sol::bases<ProjectFile>(),         //
        "title", sol::property([](CodeFile& f) { return toStd(QFileInfo(f.path()).fileName()); }), //
        "text",
        sol::property([](CodeFile& f) { return toStd(f.document()->toPlainText()); },
                      [](CodeFile& f, const std::string& text) {
                          QTextCursor cursor(f.document());
                          cursor.select(QTextCursor::Document);
                          cursor.insertText(fromStd(text));
                      }),
        "lineCount", sol::property([](CodeFile& f) { return f.document()->blockCount(); }), //
        "cursor", sol::property([](CodeFile& f) {
            QTextCursor cursor(f.document());
            cursor.movePosition(QTextCursor::End);
            return CursorRef{f.document(), cursor};
        }),
        "fromString",
        [](CodeFile& f, const std::string& text) {
            QTextCursor cursor(f.document());
            cursor.select(QTextCursor::Document);
            cursor.insertText(fromStd(text));
        },
        // insertCode(text[, position]): at the end, or at a character position.
        "insertCode",
        [](CodeFile& f, const std::string& text, sol::optional<int> position) {
            QTextCursor cursor(f.document());
            if (position)
                cursor.setPosition(std::clamp(*position, 0, f.document()->characterCount() - 1));
            else
                cursor.movePosition(QTextCursor::End);
            cursor.insertText(fromStd(text));
        },
        // insertCodeBefore(line, text) / insertCodeAfter(line, text): whole lines, 1-based.
        "insertCodeBefore",
        [](CodeFile& f, int line, const std::string& text) {
            QTextCursor cursor(f.document()->findBlockByNumber(std::max(0, line - 1)));
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.insertText(fromStd(text) + QLatin1Char('\n'));
        },
        "insertCodeAfter",
        [](CodeFile& f, int line, const std::string& text) {
            QTextBlock block = f.document()->findBlockByNumber(std::max(0, line - 1));
            if (!block.isValid())
                block = f.document()->lastBlock();
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::EndOfBlock);
            cursor.insertText(QLatin1Char('\n') + fromStd(text));
        },
        "insertCodeOnCursor",
        [](CodeFile& f, CursorRef& c, const std::string& text) {
            if (c.document != f.document())
                fail("the cursor belongs to another document");
            c.cursor.insertText(fromStd(text));
        });

    api.new_usertype<CursorRef>(
        "CodeCursor", sol::no_constructor, //
        "position",
        sol::property([](CursorRef& c) { return c.document ? c.cursor.position() : 0; },
                      [](CursorRef& c, int position) {
                          if (c.document)
                              c.cursor.setPosition(std::clamp(position, 0, c.document->characterCount() - 1));
                      }),
        "atEnd", sol::property([](CursorRef& c) { return !c.document || c.cursor.atEnd(); }),     //
        "atStart", sol::property([](CursorRef& c) { return !c.document || c.cursor.atStart(); }), //
        "selectedCode", sol::property([](CursorRef& c) {
            return c.document ? toStd(c.cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n')))
                              : std::string();
        }),
        "movePosition",
        [](CursorRef& c, int operation, sol::optional<int> mode, sol::optional<int> count) {
            return c.document && c.cursor.movePosition(QTextCursor::MoveOperation(operation),
                                                       QTextCursor::MoveMode(mode.value_or(0)), count.value_or(1));
        },
        "select",
        [](CursorRef& c, int type) {
            if (c.document)
                c.cursor.select(QTextCursor::SelectionType(type));
        },
        "clearSelection",
        [](CursorRef& c) {
            if (c.document)
                c.cursor.clearSelection();
        },
        "insertCode",
        [](CursorRef& c, const std::string& text) {
            if (c.document)
                c.cursor.insertText(fromStd(text));
        },
        "removeSelectedCode",
        [](CursorRef& c) {
            if (c.document)
                c.cursor.removeSelectedText();
        });

    // Sprites.
    api.new_usertype<SpritesRef>(
        "Sprites", sol::no_constructor,                                                                      //
        "count", sol::property([](SpritesRef& s) { return s.file ? s.file->document().spriteCount() : 0; }), //
        "palettes", sol::property([](SpritesRef& s) { return PalettesRef{s.file}; }),                        //
        "at",
        [](SpritesRef& s, int i) -> sol::optional<SpriteRef> {
            if (!s.file || i < 0 || i >= s.file->document().spriteCount())
                return sol::nullopt;
            return SpriteRef{s.file, i};
        },
        "find",
        [](SpritesRef& s, int group, int number) -> sol::optional<SpriteRef> {
            const int i = s.file ? s.file->document().findSprite(group, number) : -1;
            if (i < 0)
                return sol::nullopt;
            return SpriteRef{s.file, i};
        },
        "remove",
        [](SpritesRef& s, int i) {
            if (!s.file || i < 0 || i >= s.file->document().spriteCount())
                return false;
            SpriteDocument after = s.file->document();
            after.sprites().removeAt(i);
            s.file->apply(QStringLiteral("Remove sprite"), after);
            return true;
        },
        "clear",
        [](SpritesRef& s) {
            if (!s.file)
                return;
            SpriteDocument after = s.file->document();
            after.sprites().clear();
            s.file->apply(QStringLiteral("Clear sprites"), after);
        },
        "add", [](SpritesRef&) { fail("Sprites:add is not available"); }, "insert",
        [](SpritesRef&) { fail("Sprites:insert is not available"); });

    api.new_usertype<ImageInfo>("ImageProperties", sol::no_constructor, "format", &ImageInfo::format, "width",
                                &ImageInfo::width, "height", &ImageInfo::height);

    api.new_usertype<SpriteRef>(
        "Sprite", sol::no_constructor,                                //
        "index", sol::property([](SpriteRef& s) { return s.index; }), //
        "isEmpty", sol::property([](SpriteRef& s) {
            const SpriteEntry* e = spriteOf(s);
            return !e || e->width() == 0 || e->height() == 0;
        }),
        "group",
        sol::property([](SpriteRef& s) { return spriteOf(s) ? int(spriteOf(s)->group()) : 0; },
                      [](SpriteRef& s, int value) {
                          editSprite(s, "Sprite group",
                                     [value](SpriteEntry& e) { e.header.group = std::uint16_t(value); });
                      }),
        "number",
        sol::property([](SpriteRef& s) { return spriteOf(s) ? int(spriteOf(s)->number()) : 0; },
                      [](SpriteRef& s, int value) {
                          editSprite(s, "Sprite number",
                                     [value](SpriteEntry& e) { e.header.number = std::uint16_t(value); });
                      }),
        "x",
        sol::property([](SpriteRef& s) { return spriteOf(s) ? int(spriteOf(s)->header.axisX) : 0; },
                      [](SpriteRef& s, int value) {
                          editSprite(s, "Sprite axis",
                                     [value](SpriteEntry& e) { e.header.axisX = std::int16_t(value); });
                      }),
        "y",
        sol::property([](SpriteRef& s) { return spriteOf(s) ? int(spriteOf(s)->header.axisY) : 0; },
                      [](SpriteRef& s, int value) {
                          editSprite(s, "Sprite axis",
                                     [value](SpriteEntry& e) { e.header.axisY = std::int16_t(value); });
                      }),
        "width", sol::property([](SpriteRef& s) { return spriteOf(s) ? spriteOf(s)->width() : 0; }),   //
        "height", sol::property([](SpriteRef& s) { return spriteOf(s) ? spriteOf(s)->height() : 0; }), //
        "imageProperties", sol::property([](SpriteRef& s) {
            const SpriteEntry* e = spriteOf(s);
            if (!e)
                return ImageInfo{};
            const int depth = e->pixels ? e->pixels->payload.colorDepth : 8;
            return ImageInfo{depth > 8 ? 32 : 8, e->width(), e->height()};
        }),
        "palette", sol::property([](SpriteRef& s) -> sol::optional<PaletteRef> {
            const SpriteEntry* e = spriteOf(s);
            if (!e || !e->palette)
                return sol::nullopt;
            const int i = int(s.file->document().palettes().indexOf(e->palette));
            if (i < 0)
                return sol::nullopt;
            return PaletteRef{s.file, i};
        }),
        "parent", sol::property([](SpriteRef& s) { return SpritesRef{s.file}; }), //
        "duplicate",
        [](SpriteRef& s) {
            if (!spriteOf(s))
                fail("the sprite no longer exists");
            SpriteDocument after = s.file->document();
            after.sprites().insert(s.index + 1, after.sprites()[s.index]);
            s.file->apply(QStringLiteral("Duplicate sprite"), after);
            return SpriteRef{s.file, s.index + 1};
        },
        "crop", [](SpriteRef&) { fail("Sprite:crop is not available"); }, "setImage",
        [](SpriteRef&) { fail("Sprite:setImage is not available"); }, "splitRGB",
        [](SpriteRef&) { fail("Sprite:splitRGB is not available"); });

    api.new_usertype<PalettesRef>(
        "Palettes", sol::no_constructor, //
        "count", sol::property([](PalettesRef& p) { return p.file ? int(p.file->document().palettes().size()) : 0; }),
        "sprites", sol::property([](PalettesRef& p) { return SpritesRef{p.file}; }), //
        "at",
        [](PalettesRef& p, int i) -> sol::optional<PaletteRef> {
            if (!p.file || i < 0 || i >= p.file->document().palettes().size())
                return sol::nullopt;
            return PaletteRef{p.file, i};
        },
        "find",
        [](PalettesRef& p, int group, int number) -> sol::optional<PaletteRef> {
            const int i = p.file ? p.file->document().findPalette(group, number) : -1;
            if (i < 0)
                return sol::nullopt;
            return PaletteRef{p.file, i};
        });

    const auto [paletteCount, paletteName, paletteValue] = propertyMethods<PaletteRef>(&paletteProperties);
    api.new_usertype<PaletteRef>(
        "SpritePalette", sol::no_constructor,                          //
        "index", sol::property([](PaletteRef& p) { return p.index; }), //
        "group", sol::property([](PaletteRef& p) { return paletteOf(p) ? int(paletteOf(p)->palette.group) : 0; }),
        "number", sol::property([](PaletteRef& p) { return paletteOf(p) ? int(paletteOf(p)->palette.number) : 0; }),
        "parent", sol::property([](PaletteRef& p) { return PalettesRef{p.file}; }), //
        // The colors, a table of FF.RGB from index 1.
        "colorTable", sol::property([](PaletteRef& p, sol::this_state state) {
            sol::state_view view(state);
            sol::table table = view.create_table();
            if (const SpritePalette* palette = paletteOf(p)) {
                int i = 1;
                for (const Formats::Rgba& color : palette->palette.colors)
                    table[i++] = ScriptRgb{int(color.r), int(color.g), int(color.b)};
            }
            return table;
        }),
        "setColorTable",
        [](PaletteRef& p, sol::table colors) {
            const SpritePalette* palette = paletteOf(p);
            if (!palette)
                fail("the palette no longer exists");
            Formats::Palette values = palette->palette.colors;
            for (std::size_t i = 0; i < values.size(); ++i) {
                const sol::object entry = colors[int(i) + 1];
                if (!entry.valid() || !entry.is<ScriptRgb>())
                    continue;
                const ScriptRgb color = entry.as<ScriptRgb>();
                values[i].r = std::uint8_t(std::clamp(color.r, 0, 255));
                values[i].g = std::uint8_t(std::clamp(color.g, 0, 255));
                values[i].b = std::uint8_t(std::clamp(color.b, 0, 255));
            }
            SpriteDocument after = p.file->document();
            const SpritePalettePtr old = after.palettes()[p.index];
            const SpritePalettePtr replaced =
                SpriteDocument::makePalette(old->palette.group, old->palette.number, values);
            after.palettes()[p.index] = replaced;
            for (SpriteEntry& sprite : after.sprites()) {
                if (sprite.palette == old)
                    sprite.palette = replaced;
            }
            p.file->applyColorEdit(QStringLiteral("Palette colors"), after);
        },
        "propertyCount", paletteCount, "propertyName", paletteName, "propertyValue", paletteValue);

    // Animations, read from the AIR text each time.
    api.new_usertype<AnimationsRef>(
        "Animations", sol::no_constructor, //
        "count", sol::property([](AnimationsRef& a) { return airOf(a.file).count(); }), "at",
        [](AnimationsRef& a, int i) -> sol::optional<AnimationRef> {
            if (i < 0 || i >= airOf(a.file).count())
                return sol::nullopt;
            return AnimationRef{a.file, i};
        },
        "find",
        [](AnimationsRef& a, int number) -> sol::optional<AnimationRef> {
            const int i = airOf(a.file).indexOf(number);
            if (i < 0)
                return sol::nullopt;
            return AnimationRef{a.file, i};
        },
        "indexOf",
        [](AnimationsRef& a, AnimationRef& animation) { return animation.file == a.file ? animation.index : -1; },
        "remove",
        [](AnimationsRef& a, int i) {
            const AirDocument air = airOf(a.file);
            const AirAnimation* animation = animationOf(air, i);
            if (!animation)
                return false;
            const int first = animation->nameLine >= 0 ? animation->nameLine : animation->headerLine;
            QTextDocument* document = a.file->document();
            QTextCursor cursor(document->findBlockByNumber(first));
            const QTextBlock end = document->findBlockByNumber(animation->endLine);
            if (end.isValid())
                cursor.setPosition(end.position(), QTextCursor::KeepAnchor);
            else
                cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            return true;
        },
        "add", [](AnimationsRef&) { fail("Animations:add is not available"); }, "insert",
        [](AnimationsRef&) { fail("Animations:insert is not available"); }, "duplicate",
        [](AnimationsRef&) { fail("Animations:duplicate is not available"); });

    const auto [animationCount, animationName, animationValue] = propertyMethods<AnimationRef>(&animationProperties);
    auto withAnimation = [](const AnimationRef& r, auto get) {
        const AirDocument air = airOf(r.file);
        const AirAnimation* a = animationOf(air, r.index);
        if (!a)
            fail("the animation no longer exists");
        return get(*a);
    };
    api.new_usertype<AnimationRef>(
        "AnimationCode", sol::no_constructor, //
        "number", sol::property([withAnimation](AnimationRef& r) {
            return withAnimation(r, [](const AirAnimation& a) { return a.number; });
        }),
        "displayName", sol::property([withAnimation](AnimationRef& r) {
            return withAnimation(r, [](const AirAnimation& a) { return toStd(a.caption()); });
        }),
        "text", sol::property([withAnimation](AnimationRef& r) {
            return withAnimation(r, [](const AirAnimation& a) { return toStd(a.name); });
        }),
        "code", sol::property([withAnimation](AnimationRef& r) {
            return withAnimation(r, [&r](const AirAnimation& a) {
                return toStd(linesText(r.file, a.nameLine >= 0 ? a.nameLine : a.headerLine, a.lastLine));
            });
        }),
        "frameCount", sol::property([withAnimation](AnimationRef& r) {
            return withAnimation(r, [](const AirAnimation& a) { return int(a.frames.size()); });
        }),
        "frames", sol::property([withAnimation](AnimationRef& r, sol::this_state state) {
            const int count = withAnimation(r, [](const AirAnimation& a) { return int(a.frames.size()); });
            QList<FrameRef> frames;
            for (int i = 0; i < count; ++i)
                frames << FrameRef{r.file, r.index, i};
            return listOf(state, frames);
        }),
        "frameAt",
        [withAnimation](AnimationRef& r, int i) -> sol::optional<FrameRef> {
            const int count = withAnimation(r, [](const AirAnimation& a) { return int(a.frames.size()); });
            if (i < 0 || i >= count)
                return sol::nullopt;
            return FrameRef{r.file, r.index, i};
        },
        "frameIndex",
        [](AnimationRef& r, FrameRef& f) { return f.file == r.file && f.animation == r.index ? f.frame : -1; },
        "propertyCount", animationCount, "propertyName", animationName, "propertyValue", animationValue);

    const auto [frameCount, frameName, frameValue] = propertyMethods<FrameRef>(&frameProperties);
    auto withFrame = [](const FrameRef& r, auto get) {
        const AirDocument air = airOf(r.file);
        const AirFrame* f = frameOf(air, r.animation, r.frame);
        if (!f)
            fail("the frame no longer exists");
        return get(*f);
    };
    api.new_usertype<FrameRef>(
        "AnimationFrameCode", sol::no_constructor, //
        "group",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.group; }); }),
        "number",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.number; }); }),
        "offsetX",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.x; }); }),
        "offsetY",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.y; }); }), "time",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.time; }); }),
        "scaleX",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.xScale; }); }),
        "scaleY",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.yScale; }); }),
        "angle",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.angle; }); }),
        "flip", sol::property([withFrame](FrameRef& r) {
            return withFrame(r, [](const AirFrame& f) { return (f.flipH ? 1 : 0) | (f.flipV ? 2 : 0); });
        }),
        "loopStart",
        sol::property([withFrame](FrameRef& r) { return withFrame(r, [](const AirFrame& f) { return f.loopStart; }); }),
        "offset", sol::property([withFrame](FrameRef& r, sol::this_state state) {
            const auto [x, y] = withFrame(r, [](const AirFrame& f) { return std::pair(f.x, f.y); });
            sol::state_view view(state);
            return view.create_table_with("x", x, "y", y);
        }),
        "scale", sol::property([withFrame](FrameRef& r, sol::this_state state) {
            const auto [x, y] = withFrame(r, [](const AirFrame& f) { return std::pair(f.xScale, f.yScale); });
            sol::state_view view(state);
            return view.create_table_with("x", x, "y", y);
        }),
        "blend", sol::property([withFrame](FrameRef& r) {
            return withFrame(r, [](const AirFrame& f) {
                return BlendInfo{blendMode(f.transparency), f.transparency.source / 256.0, f.transparency.dest / 256.0};
            });
        }),
        "attackBoxes", [](FrameRef& r) { return BoxesRef{r.file, r.animation, r.frame, 1}; }, "collisionBoxes",
        [](FrameRef& r) { return BoxesRef{r.file, r.animation, r.frame, 2}; }, "propertyCount", frameCount,
        "propertyName", frameName, "propertyValue", frameValue);

    api.new_usertype<BlendInfo>("AnimationFrameBlend", sol::no_constructor, "mode", sol::readonly(&BlendInfo::mode),
                                "source", sol::readonly(&BlendInfo::source), "dest", sol::readonly(&BlendInfo::dest));

    auto boxesOf = [](const auto& r, bool* isDefault) {
        const AirDocument air = airOf(r.file);
        const AirAnimation* a = animationOf(air, r.animation);
        if (!a || r.frame < 0 || r.frame >= a->frames.size())
            return QList<AirBox>();
        return a->boxes(r.frame, r.type, isDefault);
    };
    api.new_usertype<BoxesRef>(
        "HitBoxes", sol::no_constructor, //
        "boxType", sol::property([boxesOf](BoxesRef& r) {
            bool isDefault = false;
            boxesOf(r, &isDefault);
            return r.type | (isDefault ? 4 : 0);
        }),
        "count", sol::property([boxesOf](BoxesRef& r) { return int(boxesOf(r, nullptr).size()); }), "at",
        [boxesOf](BoxesRef& r, int i) -> sol::optional<BoxRef> {
            if (i < 0 || i >= boxesOf(r, nullptr).size())
                return sol::nullopt;
            return BoxRef{r.file, r.animation, r.frame, r.type, i};
        },
        "add", [](BoxesRef&) { fail("HitBoxes:add is not available"); }, "remove",
        [](BoxesRef&) { fail("HitBoxes:remove is not available"); }, "clear",
        [](BoxesRef&) { fail("HitBoxes:clear is not available"); });

    auto boxOf = [boxesOf](const BoxRef& r) {
        const QList<AirBox> boxes = boxesOf(r, nullptr);
        if (r.index < 0 || r.index >= boxes.size())
            fail("the box no longer exists");
        return boxes[r.index];
    };
    api.new_usertype<BoxRef>("HitBoxCode", sol::no_constructor, //
                             "boxType", sol::property([](BoxRef& r) { return r.type; }), "code",
                             sol::property([boxOf](BoxRef& r) { return toStd(lineText(r.file, boxOf(r).line)); }),
                             "left", sol::property([boxOf](BoxRef& r) { return boxOf(r).left; }), "top",
                             sol::property([boxOf](BoxRef& r) { return boxOf(r).top; }), "right",
                             sol::property([boxOf](BoxRef& r) { return boxOf(r).right; }), "bottom",
                             sol::property([boxOf](BoxRef& r) { return boxOf(r).bottom; }), "rect",
                             sol::property([boxOf](BoxRef& r, sol::this_state state) {
                                 const AirBox b = boxOf(r);
                                 sol::state_view view(state);
                                 return view.create_table_with("left", b.left, "top", b.top, "right", b.right, "bottom",
                                                               b.bottom);
                             }));

    // Sounds. FFS gives scripts an unusable object here.
    api.new_usertype<SoundsRef>(
        "Sounds", sol::no_constructor, //
        "count", sol::property([](SoundsRef& s) { return s.file ? s.file->document().count() : 0; }), "at",
        [](SoundsRef& s, int i) -> sol::optional<SoundRef> {
            if (!s.file || i < 0 || i >= s.file->document().count())
                return sol::nullopt;
            return SoundRef{s.file, i};
        },
        "find",
        [](SoundsRef& s, int group, int number) -> sol::optional<SoundRef> {
            const int i = s.file ? s.file->document().find(group, number) : -1;
            if (i < 0)
                return sol::nullopt;
            return SoundRef{s.file, i};
        });
    auto soundOf = [](const SoundRef& r) -> const SoundEntry& {
        if (!r.file || r.index < 0 || r.index >= r.file->document().count())
            fail("the sound no longer exists");
        return r.file->document().sounds()[r.index];
    };
    api.new_usertype<SoundRef>("Sound", sol::no_constructor, //
                               "index", sol::property([](SoundRef& r) { return r.index; }), "group",
                               sol::property([soundOf](SoundRef& r) { return soundOf(r).group; }), "number",
                               sol::property([soundOf](SoundRef& r) { return soundOf(r).number; }), "size",
                               sol::property([soundOf](SoundRef& r) {
                                   const SoundEntry& e = soundOf(r);
                                   return e.data ? qint64(e.data->size()) : qint64(0);
                               }));

    api.new_usertype<Unreachable<0>>("CodeNode", sol::no_constructor);
    api.new_usertype<Unreachable<1>>("ParentCode", sol::no_constructor);
    api.new_usertype<Unreachable<2>>("GroupCode", sol::no_constructor);
    api.new_usertype<Unreachable<3>>("ValueCode", sol::no_constructor);
    api.new_usertype<Unreachable<4>>("NamedCode", sol::no_constructor);
    api.new_usertype<Unreachable<5>>("Organizable", sol::no_constructor);
    api.new_usertype<Unreachable<6>>("PropertyItem", sol::no_constructor);
    api.new_usertype<Unreachable<7>>("Progress", sol::no_constructor);
    api.new_usertype<Unreachable<8>>("Debugger", sol::no_constructor);
    api.new_usertype<Unreachable<9>>("DebuggerBase", sol::no_constructor);
    api.new_usertype<Unreachable<10>>("Emulator", sol::no_constructor);
    api.new_usertype<Unreachable<11>>("Variable", sol::no_constructor);
}

} // namespace OFS::Script
