#include "shell/SaveChecks.h"

#include "core/Version.h"
#include "core/settings/Config.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/PaletteEdits.h"
#include "core/sprites/SpriteFile.h"

#include <QCoreApplication>
#include <QMessageBox>

namespace OFS {

namespace {

// Translation context for this file's texts (lupdate reads the class's qualified name).
struct SaveCheckTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::SaveCheckTexts)
};

bool refuse(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox::warning(parent, title, text);
    return false;
}

} // namespace

bool confirmSave(ProjectFile* file, Config& config, QWidget* parent)
{
    if (auto* sprites = qobject_cast<SpriteFile*>(file)) {
        const QString title = SaveCheckTexts::tr("Save SFF");
        if (sprites->document().spriteCount() == 0) {
            if (config.boolValue(QStringLiteral("showmsg_sprs_empty"), false))
                return refuse(parent, title,
                              SaveCheckTexts::tr("You can't save an empty SFF because M.U.G.E.N. will not open it."));
            return true;
        }
        const int missing = int(PaletteEdits::spritesWithoutPalette(sprites->document()).size());
        if (missing == 0)
            return true;
        if (config.boolValue(QStringLiteral("showmsg_spr_no_pal"), false))
            return refuse(parent, title, SaveCheckTexts::tr("You can't save this SFF without palettes."));
        const QString text =
            SaveCheckTexts::tr("Found %1 error(s). Do you want to let %2 automatically fix them?\n"
                               "Choosing 'Yes' will convert all sprites to a supported format (data loss may occur) "
                               "for current SFF version (you can change current version by clicking on it on status "
                               "bar).\n'No' will cancel this operation and you can manually fix the issues.")
                .arg(missing)
                .arg(QStringLiteral(OFS_DISPLAY_NAME));
        if (QMessageBox::warning(parent, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) !=
            QMessageBox::Yes)
            return false;
        SpriteDocument fixed = sprites->document();
        PaletteEdits::addMissingPalette(fixed);
        sprites->apply(SaveCheckTexts::tr("Fix sprites without palette"), fixed);
        return true;
    }
    if (auto* sounds = qobject_cast<SoundFile*>(file)) {
        if (sounds->document().count() == 0 && config.boolValue(QStringLiteral("showmsg_snds_empty"), false))
            return refuse(parent, SaveCheckTexts::tr("Save SND"),
                          SaveCheckTexts::tr("You can't save an empty SND because M.U.G.E.N. will not open it."));
        return true;
    }
    return true;
}

} // namespace OFS
