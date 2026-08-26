#pragma once

#include <QDialog>
#include <QImage>

#include <functional>

class QCheckBox;
class QLabel;
class QListWidget;
class QScrollBar;
class QSlider;
class QSpinBox;
class QTimer;

namespace OFS {
class SpriteFile;
}

namespace OFS::Mugen {

struct PalFX
{
    int add[3] = {0, 0, 0};
    int mul[3] = {256, 256, 256};
    int sin[3] = {0, 0, 0};
    int period = 1;
    int color = 256;
    bool invert = false;

    QRgb apply(QRgb color, int tick) const;
    QString controllerText() const;
};

bool readPalFXTemplate(const QString& path, PalFX* palfx);

class PalFXDialog : public QDialog
{
    Q_OBJECT

public:
    PalFXDialog(SpriteFile* sprites, int sprite, const QString& templateDir, QWidget* parent = nullptr);

    PalFX values() const;
    void setValues(const PalFX& palfx);
    void setInsertHandler(std::function<void(const QString&)> insert) { m_insert = std::move(insert); }
    QImage preview() const;

private:
    void updatePreview();

    SpriteFile* m_sprites;
    QScrollBar* m_selector;
    QLabel* m_selectorLabel;
    QSpinBox* m_add[3];
    QSpinBox* m_mul[3];
    QSpinBox* m_sin[3];
    QSpinBox* m_period;
    QSpinBox* m_color;
    QCheckBox* m_invert;
    QListWidget* m_templates;
    QLabel* m_preview;
    QSlider* m_zoom;
    QTimer* m_timer;
    int m_tick = 0;
    std::function<void(const QString&)> m_insert;
};

} // namespace OFS::Mugen
