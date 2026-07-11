#pragma once

#include <QDialog>
#include <QPointer>

class QAction;
class QListView;
class QPushButton;

namespace OFS {

class ClipboardStore;

// View > Clipboard container: keeps named copies of clipboard contents for the session. Cut and Copy run the editor's
// action first, then store the clipboard.
class ClipboardContainer : public QDialog
{
    Q_OBJECT

public:
    explicit ClipboardContainer(QWidget* parent = nullptr);
    ~ClipboardContainer() override;

    static ClipboardContainer* showClipboard(QWidget* parent = nullptr);
    static ClipboardContainer* instance();

    ClipboardStore& store() const { return *m_store; }

    // Defaults to the registered edit_cut, edit_copy and edit_paste actions.
    void setEditActions(QAction* cut, QAction* copy, QAction* paste);

    // Returns the new row, or -1 when the clipboard is empty.
    int addCurrentClipboard(const QString& name = {});
    bool putBack(int row);

    void cut();
    void copy();
    void paste();
    void removeSelected();
    void renameSelected();
    void clearAll();

    int currentRow() const;
    void setCurrentRow(int row);

Q_SIGNALS:
    // For the checkable View > Clipboard container action.
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QAction* editAction(const QPointer<QAction>& explicitAction, const char* id) const;
    void captureAfter(QAction* action);
    void updateButtons();
    void showContextMenu(const QPoint& pos);

    ClipboardStore* m_store;
    QListView* m_list;
    QPushButton* m_paste;
    QPushButton* m_remove;
    QPointer<QAction> m_cutAction;
    QPointer<QAction> m_copyAction;
    QPointer<QAction> m_pasteAction;
    bool m_customActions = false;
};

} // namespace OFS
