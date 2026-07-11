#pragma once

#include <QDialog>
#include <QHash>
#include <QKeySequence>
#include <QMap>
#include <QStringList>

#include <functional>
#include <vector>

class QAbstractButton;
class QButtonGroup;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QTabWidget;

namespace OFS {

class AppContext;
class ColorSet;
class ColorsPage;
class OptionsPage;
class ShortcutEditor;

// Tools > Options. The controls are a working copy of the settings until Apply, which writes config.xml and
// shortcuts.xml and emits settingsApplied(). The engine tab stores its changes at once.
// Each control's objectName is its config.xml key (see the binding table in OptionsDialog.cpp).
class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    enum Tab
    {
        GeneralTab,
        InterfaceTab,
        MessagesTab,
        ColorsTab,
        GraphicsTab,
        CodeTab,
        EngineTab,
    };

    // Called when a key sequence is taken from another action in an overlapping scope. Return true to move it.
    using ConflictHandler = std::function<bool(const QString& id, const QKeySequence& keys, const QStringList& others)>;

    explicit OptionsDialog(AppContext& context, QWidget* parent = nullptr);
    ~OptionsDialog() override;

    // Returns QDialog::exec()'s result, or Rejected when there is no context.
    static int showOptions(QWidget* parent = nullptr, const std::function<void()>& onApplied = {},
                           int tab = GeneralTab);

    void load();
    void loadDefaults();
    // Safe to call any number of times. Returns false when a file could not be written.
    bool apply();
    bool exportSettings(const QString& path, QString* error = nullptr) const;
    bool importSettings(const QString& path, QString* error = nullptr);
    // Saves as <userDir>/colorsets/<name>.xml and selects it.
    bool saveColorSet(const QString& name, QString* error = nullptr);

    void setConflictHandler(ConflictHandler handler);

    QTabWidget* tabs() const { return m_tabs; }
    ShortcutEditor* shortcutEditor() const { return m_shortcuts; }
    ColorsPage* colorsPage() const { return m_colors; }
    OptionsPage* enginePage(const QString& engine) const { return m_enginePages.value(engine); }
    QComboBox* engineSelector() const { return m_engines; }

    QStringList boundKeys() const;

    static QString customColorSetName();

Q_SIGNALS:
    void settingsApplied();

private:
    struct Binding;
    using ValueSource = std::function<QString(const QString& key, const QString& fallback)>;

    QWidget* createGeneralTab();
    QWidget* createInterfaceTab();
    QWidget* createMessagesTab();
    QWidget* createColorsTab();
    QWidget* createGraphicsTab();
    QWidget* createCodeTab();
    void createEnginePages();
    QLayout* createButtonRow();

    void bind(const QStringList& keys, const QStringList& fallbacks, std::function<void(const QStringList&)> read,
              std::function<QStringList()> write);
    void bind(const QString& key, const QString& fallback, std::function<void(const QString&)> read,
              std::function<QString()> write);
    void bindCheck(QAbstractButton* box, const QString& key, bool fallback);
    void bindSpin(QSpinBox* spin, const QString& key, int fallback);
    void bindDoubleSpin(QDoubleSpinBox* spin, const QString& key, double fallback);
    void bindLine(QLineEdit* edit, const QString& key);
    void bindIndex(QComboBox* combo, const QString& key, int fallbackValue, int offset = 0);
    void bindAnswer(QComboBox* combo, const QString& key);
    void bindFlag(QAbstractButton* box, const QString& key);
    void bindRadio(QButtonGroup* group, const QString& key, int fallback);
    void bindPercent(QSlider* slider, const QString& key, double fallback);
    void bindName(QComboBox* combo, const QString& key, const QString& fallback);

    void readBindings(const ValueSource& source);
    QString currentValue(const QString& key, const QString& fallback) const;
    QString shippedValue(const QString& key, const QString& fallback) const;
    QMap<QString, QString> boundValues() const;

    QComboBox* answerCombo(const QString& key);
    QPushButton* browseButton(QLineEdit* target, const QString& title);
    void refreshThemeLists();
    void refreshLanguageInfo();
    void showColorSet(const QString& name);
    void colorsEdited();
    void colorSaveRequested();
    QString customColorSetPath() const;
    void loadShortcutTables(bool defaults);
    void selectEngine(int index);
    void openClicked();
    void saveClicked();
    void applyClicked();
    void updateDependentControls();

    AppContext& m_context;
    std::vector<Binding> m_bindings;
    QTabWidget* m_tabs = nullptr;
    ShortcutEditor* m_shortcuts = nullptr;
    ColorsPage* m_colors = nullptr;
    QComboBox* m_engines = nullptr;
    QStackedWidget* m_engineStack = nullptr;
    QWidget* m_engineTab = nullptr;
    QMap<QString, OptionsPage*> m_enginePages;
    QStringList m_engineOrder;
    QPushButton* m_applyButton = nullptr;

    QComboBox* m_preset = nullptr;
    QComboBox* m_iconset = nullptr;
    QComboBox* m_stylesheet = nullptr;
    QComboBox* m_colorset = nullptr;
    QComboBox* m_language = nullptr;
    QLabel* m_languageAuthor = nullptr;
    QLabel* m_languageContact = nullptr;
    QFontComboBox* m_fontName = nullptr;
    QComboBox* m_renderer = nullptr;
    QButtonGroup* m_crosshair = nullptr;
    QButtonGroup* m_background = nullptr;
    QList<QWidget*> m_crosshairFiles;
    QList<QWidget*> m_backgroundFiles;
    QAbstractButton* m_dpiAuto = nullptr;
    QDoubleSpinBox* m_dpiScale = nullptr;

    QString m_loadedFont;
    bool m_fontTouched = false;
    QString m_loadedOpenGlMode;
    bool m_syncingColors = false;
    bool m_shortcutsReset = false;
    bool m_applying = false;
    // Values of an opened settings file that no control shows, written on Apply.
    QMap<QString, QString> m_extraConfig;
    QMap<QString, QString> m_extraShortcuts;
};

} // namespace OFS
