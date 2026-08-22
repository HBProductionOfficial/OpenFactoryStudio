#pragma once

#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteImages.h"
#include "formats/image/Resize.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QRadioButton;
class QSpinBox;
class QToolButton;

namespace OFS {

QString spriteCaption(const SpriteEntry& entry);

// Range picker shared by the Range and Resize dialogs.
class SpriteRange : public QObject
{
    Q_OBJECT

public:
    // Builds the controls into `dialog` with the object names cur, all, grp, groupBox.grpNum, groupBox.interval,
    // groupBox.start and groupBox.end.
    SpriteRange(QDialog* dialog, int currentGroup);

    QWidget* widget() const { return m_widget; }
    QList<int> sprites(const SpriteDocument& doc, int current) const;

private:
    QWidget* m_widget;
    QRadioButton* m_current;
    QRadioButton* m_all;
    QRadioButton* m_group;
    QSpinBox* m_groupNumber;
    QCheckBox* m_interval;
    QSpinBox* m_start;
    QSpinBox* m_end;
};

// Sprites > Crop image and Save image (aligned): which sprites.
class RangeDialog : public QDialog
{
    Q_OBJECT

public:
    RangeDialog(int currentGroup, QWidget* parent = nullptr);
    QList<int> sprites(const SpriteDocument& doc, int current) const { return m_range->sprites(doc, current); }

private:
    SpriteRange* m_range;
};

class ResizeDialog : public QDialog
{
    Q_OBJECT

public:
    ResizeDialog(int currentGroup, QWidget* parent = nullptr);
    QList<int> sprites(const SpriteDocument& doc, int current) const { return m_range->sprites(doc, current); }
    double widthScale() const;
    double heightScale() const;
    Formats::ResizeFilter filter() const;

private:
    SpriteRange* m_range;
    QDoubleSpinBox* m_width;
    QDoubleSpinBox* m_height;
    QComboBox* m_filter;
};

class GroupDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Kind
    {
        Delete,
        Rename,
    };
    GroupDialog(Kind kind, int currentGroup, QWidget* parent = nullptr);

    int group() const;
    // -1, -1 for the entire group.
    int first() const;
    int last() const;
    int newGroup() const;

private:
    QSpinBox* m_group;
    QRadioButton* m_entire;
    QSpinBox* m_start;
    QSpinBox* m_end;
    QSpinBox* m_newGroup = nullptr;
};

class AlignmentDialog : public QDialog
{
    Q_OBJECT

public:
    AlignmentDialog(const SpriteDocument& doc, int current, QWidget* parent = nullptr);

Q_SIGNALS:
    void applyRequested(const QList<int>& sprites, int dx, int dy);

private:
    QSpinBox* m_x;
    QSpinBox* m_y;
    QListWidget* m_list;
};

class ItemsDialog : public QDialog
{
    Q_OBJECT

public:
    ItemsDialog(const SpriteDocument& doc, int current, QWidget* parent = nullptr);
    QList<int> sprites() const;

private:
    QListWidget* m_list;
};

class SpriteFormatDialog : public QDialog
{
    Q_OBJECT

public:
    SpriteFormatDialog(Formats::SffVersion current, QWidget* parent = nullptr);
    Formats::SffVersion version() const;

private:
    QRadioButton* m_beta;
    QRadioButton* m_v10;
    QRadioButton* m_v11;
};

// Sprites > Add. It starts from the defaults every time.
class AddSpritesDialog : public QDialog
{
    Q_OBJECT

public:
    AddSpritesDialog(const QPoint& currentAxis, QWidget* parent = nullptr);
    AddSpritesOptions options() const;

private:
    void updateEnabled();

    QCheckBox* m_detect;
    QSpinBox* m_group;
    QSpinBox* m_index;
    QRadioButton* m_tens;
    QList<QToolButton*> m_anchors;
    QSpinBox* m_axisX;
    QSpinBox* m_axisY;
    QCheckBox* m_reversed;
    QComboBox* m_palette;
    QSpinBox* m_paletteGroup;
    QSpinBox* m_paletteIndex;
    QComboBox* m_crop;
    QComboBox* m_share;
};

class ImagePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    ImagePropertiesDialog(bool indexed, int width, int height, QWidget* parent = nullptr);
    bool indexed() const;
    int canvasWidth() const;
    int canvasHeight() const;

private:
    QComboBox* m_format;
    QSpinBox* m_width;
    QSpinBox* m_height;
};

class RotateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RotateDialog(QWidget* parent = nullptr);
    double angle() const;
    int filter() const; // Nearest Neighbor, Bilinear, EPX, HQ

private:
    QDoubleSpinBox* m_angle;
    QComboBox* m_filter;
};

class SpriteExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpriteExportDialog(QWidget* parent = nullptr);
    // -1 for all sprites.
    int group() const;

private:
    QRadioButton* m_all;
    QSpinBox* m_group;
};

} // namespace OFS
