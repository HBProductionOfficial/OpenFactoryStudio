#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QListWidget;
class QRadioButton;
class QSpinBox;

namespace OFS {

// Sounds > Add and Palettes > Add from files: the identifiers of the added items.
class AddItemsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Kind
    {
        Sounds,
        Palettes,
        EditorPalettes,
    };
    AddItemsDialog(Kind kind, QWidget* parent = nullptr);

    bool detect() const;
    int group() const;
    int number() const;
    int step() const;
    bool reversed() const;

private:
    QCheckBox* m_detect = nullptr;
    QSpinBox* m_group;
    QSpinBox* m_number;
    QRadioButton* m_ten;
    QCheckBox* m_reversed = nullptr;
};

class ResampleDialog : public QDialog
{
    Q_OBJECT

public:
    ResampleDialog(const QStringList& sounds, int current, int channels, int sampleRate, int bitsPerSample,
                   QWidget* parent = nullptr);

    QList<int> sounds() const;
    int channels() const;
    int sampleRate() const;
    int bitsPerSample() const;
    static QList<int> sampleRates();

private:
    QListWidget* m_list;
    QComboBox* m_channels;
    QComboBox* m_rate;
    QComboBox* m_bits;
};

} // namespace OFS
