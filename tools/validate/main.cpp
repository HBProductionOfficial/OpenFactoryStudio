// ofs-validate: checks the code files of M.U.G.E.N projects against the syntax database, the same
// way the code editors do, and prints the problems found.
//
//   ofs-validate [--strict] [--quiet] <project.def>...
//   ofs-validate --corpus-env [--strict] [--quiet]
//
// Every code file of each project (definitions, commands, states) is checked with the project's
// version. Exit code: 0 when no errors were found (no issues at all with --strict), 1 otherwise,
// 2 on usage or loading errors.
//
// --corpus-env checks the stock files of the local test corpus instead: Kung Fu Man from
// OFS_CORPUS_KFM (its folder) and, from the M.U.G.E.N installation in OFS_CORPUS_MUGEN, the
// screens in data/ and data/kfm/ plus every stage and font definition. It exits with 77 (skipped)
// when neither variable is set.

#include "core/editors/EditorInfo.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/syntax/SyntaxDatabase.h"
#include "engines/mugen/MugenEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <memory>

using namespace OFS;

namespace {

const char* severityName(Issue::Severity severity)
{
    switch (severity) {
    case Issue::Severity::Error:
        return "error";
    case Issue::Severity::Warning:
        return "warning";
    case Issue::Severity::Deprecation:
        return "deprecation";
    case Issue::Severity::Information:
        break;
    }
    return "info";
}

QStringList corpusProjects()
{
    QStringList defs;
    const QString kfm = qEnvironmentVariable("OFS_CORPUS_KFM");
    if (!kfm.isEmpty() && QFileInfo::exists(kfm + QStringLiteral("/kfm.def")))
        defs << kfm + QStringLiteral("/kfm.def");
    const QString mugen = qEnvironmentVariable("OFS_CORPUS_MUGEN");
    if (!mugen.isEmpty()) {
        const QDir root(mugen);
        QStringList folders = {QStringLiteral("data"), QStringLiteral("data/kfm"), QStringLiteral("stages"),
                               QStringLiteral("font")};
        // Every character of the engine folder (chars/<name>/<name>.def).
        const QDir chars(root.filePath(QStringLiteral("chars")));
        for (const QString& name : chars.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            folders << QStringLiteral("chars/") + name;
        for (const QString& folder : std::as_const(folders)) {
            const QDir dir(root.filePath(folder));
            for (const QString& name : dir.entryList({QStringLiteral("*.def")}, QDir::Files, QDir::Name))
                defs << dir.filePath(name);
        }
    }
    return defs;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    const bool strict = args.removeAll(QStringLiteral("--strict")) > 0;
    const bool quiet = args.removeAll(QStringLiteral("--quiet")) > 0;
    if (args.removeAll(QStringLiteral("--corpus-env")) > 0) {
        args = corpusProjects();
        if (args.isEmpty()) {
            QTextStream(stdout) << "OFS_CORPUS_KFM / OFS_CORPUS_MUGEN not set: skipped\n";
            return 77;
        }
    }
    QTextStream out(stdout);
    QTextStream err(stderr);
    if (args.isEmpty() || args.contains(QStringLiteral("--help"))) {
        err << "usage: ofs-validate [--strict] [--quiet] <project.def>...\n";
        return 2;
    }

    Mugen::MugenEngine engine;
    const SyntaxDatabase* syntax = engine.syntax();
    if (!syntax || syntax->isEmpty()) {
        err << "the syntax database could not be loaded from " << QDir::toNativeSeparators(engine.dataDir()) << "\n";
        return 2;
    }

    int files = 0;
    int errors = 0;
    int others = 0;
    bool loadFailed = false;
    for (const QString& path : std::as_const(args)) {
        QString error;
        std::unique_ptr<Project> project(engine.openProject(path, &error));
        if (!project) {
            err << QDir::toNativeSeparators(path) << ": " << error << "\n";
            loadFailed = true;
            continue;
        }
        for (const QString& editorId : project->editorIds()) {
            if (engine.editor(editorId).workspace != WorkspaceType::Code)
                continue;
            for (const QString& file : project->editorFiles(editorId)) {
                QFile f(file);
                if (!f.open(QIODevice::ReadOnly))
                    continue; // missing files are reported by the editors, not here
                QString encoding;
                QByteArray bom;
                QString text = CodeFile::decode(f.readAll(), &encoding, &bom);
                text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
                ++files;
                for (const Issue& issue : engine.validate(*project, editorId, text)) {
                    if (issue.severity == Issue::Severity::Error)
                        ++errors;
                    else
                        ++others;
                    if (!quiet)
                        out << QDir::toNativeSeparators(file) << ':' << issue.line + 1 << ':' << issue.column + 1
                            << ": " << severityName(issue.severity) << ": " << issue.message << "\n";
                }
            }
        }
    }
    out << files << " file(s) checked: " << errors << " error(s), " << others << " other issue(s)\n";
    if (loadFailed)
        return 2;
    return (errors > 0 || (strict && others > 0)) ? 1 : 0;
}
