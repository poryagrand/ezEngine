#include <GuiFoundation/PCH.h>
#include <GuiFoundation/Dialogs/ShortcutEditorDlg.moc.h>
#include <GuiFoundation/Action/ActionManager.h>
#include <CoreUtils/Localization/TranslationLookup.h>
#include <QTableWidget>
#include <QTreeWidget>
#include <QKeySequenceEdit>

ezShortcutEditorDlg::ezShortcutEditorDlg(QWidget* parent) : QDialog(parent)
{
  setupUi(this);

  EZ_VERIFY(connect(Shortcuts, SIGNAL(itemSelectionChanged()), this, SLOT(SlotSelectionChanged())) != nullptr, "signal/slot connection failed");

  m_iSelectedAction = -1;
  KeyEditor->setEnabled(false);

  ezMap<ezString, ezMap<ezString, ezInt32>> SortedItems;

  {
    auto itActions = ezActionManager::GetActionIterator();

    while (itActions.IsValid())
    {
      if (itActions.Value()->m_Type == ezActionType::Action)
      {
        SortedItems[itActions.Value()->m_sCategoryPath][itActions.Value()->m_sActionName] = m_ActionDescs.GetCount();
        m_ActionDescs.PushBack(itActions.Value());
      }

      itActions.Next();
    }
  }

  {
    QtScopedBlockSignals bs(Shortcuts);
    QtScopedUpdatesDisabled ud(Shortcuts);

    Shortcuts->setAlternatingRowColors(true);
    Shortcuts->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
    Shortcuts->setExpandsOnDoubleClick(true);

    Shortcuts->setColumnCount(3);

    for (auto it = SortedItems.GetIterator(); it.IsValid(); ++it)
    {
      auto pParent = new QTreeWidgetItem();
      pParent->setData(0, Qt::DisplayRole, it.Key().GetData());
      Shortcuts->addTopLevelItem(pParent);

      pParent->setExpanded(true);
      pParent->setFirstColumnSpanned(true);
      pParent->setFlags(Qt::ItemFlag::ItemIsEnabled);

      QFont font = pParent->font(0);
      font.setBold(true);

      pParent->setFont(0, font);

      for (auto idx : it.Value())
      {
        const auto& item = m_ActionDescs[idx];
        auto pItem = new QTreeWidgetItem(pParent);
        pItem->setData(0, Qt::UserRole, idx);
        pItem->setData(0, Qt::DisplayRole, item->m_sActionName.GetData());
        pItem->setData(1, Qt::DisplayRole, ezTranslate(item->m_sActionName));
        pItem->setData(2, Qt::DisplayRole, item->m_sShortcut.GetData());
      }
    }

    Shortcuts->resizeColumnToContents(0);
    Shortcuts->resizeColumnToContents(2);
  }
}

ezShortcutEditorDlg::~ezShortcutEditorDlg()
{
  ezActionManager::SaveShortcutAssignment();
}

void ezShortcutEditorDlg::UpdateTable(bool bOnlyShortcuts)
{
  for (ezInt32 iTop = 0; iTop < Shortcuts->topLevelItemCount(); ++iTop)
  {
    auto pTopItem = Shortcuts->topLevelItem(iTop);

    for (ezInt32 iChild = 0; iChild < pTopItem->childCount(); ++iChild)
    {
      auto pChild = pTopItem->child(iChild);

      ezInt32 idx = pChild->data(0, Qt::UserRole).toInt();

      const auto& item = m_ActionDescs[idx];

      pChild->setData(2, Qt::DisplayRole, QVariant(item->m_sShortcut.GetData()));
    }
  }

  if (m_iSelectedAction >= 0)
    ButtonRemove->setEnabled(!m_ActionDescs[m_iSelectedAction]->m_sShortcut.IsEmpty());

  QString sText = KeyEditor->keySequence().toString(QKeySequence::SequenceFormat::NativeText);
  ButtonAssign->setEnabled(!sText.isEmpty());
}

void ezShortcutEditorDlg::SlotSelectionChanged()
{
  auto selection = Shortcuts->selectedItems();
  if (selection.size() == 1)
  {
    m_iSelectedAction = selection[0]->data(0, Qt::UserRole).toInt();
    KeyEditor->clear();
    KeyEditor->setEnabled(true);
    ButtonAssign->setEnabled(false);
    ButtonRemove->setEnabled(!m_ActionDescs[m_iSelectedAction]->m_sShortcut.IsEmpty());
  }
  else
  {
    m_iSelectedAction = -1;
    KeyEditor->clear();
    KeyEditor->setEnabled(false);
    ButtonAssign->setEnabled(false);
    ButtonRemove->setEnabled(false);
  }
}

void ezShortcutEditorDlg::on_KeyEditor_editingFinished()
{
  UpdateKeyEdit();
}

void ezShortcutEditorDlg::UpdateKeyEdit()
{
  if (m_iSelectedAction < 0)
    return;

  QString sText = KeyEditor->keySequence().toString(QKeySequence::SequenceFormat::NativeText);
  ButtonAssign->setEnabled(!sText.isEmpty());
}

void ezShortcutEditorDlg::on_KeyEditor_keySequenceChanged(const QKeySequence & keySequence)
{
  UpdateKeyEdit();
}

void ezShortcutEditorDlg::on_ButtonAssign_clicked()
{
  QString sText = KeyEditor->keySequence().toString(QKeySequence::SequenceFormat::NativeText);
  KeyEditor->clear();

  m_ActionDescs[m_iSelectedAction]->m_sShortcut = sText.toUtf8().data();
  m_ActionDescs[m_iSelectedAction]->UpdateExistingActions();

  UpdateTable(true);
}

void ezShortcutEditorDlg::on_ButtonRemove_clicked()
{
  KeyEditor->clear();

  m_ActionDescs[m_iSelectedAction]->m_sShortcut.Clear();
  m_ActionDescs[m_iSelectedAction]->UpdateExistingActions();

  UpdateTable(true);
}
