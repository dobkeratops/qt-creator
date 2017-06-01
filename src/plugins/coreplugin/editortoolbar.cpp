/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "editortoolbar.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/editormanager_p.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/fileiconprovider.h>
#include <coreplugin/icore.h>

#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/utilsicons.h>

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDrag>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <QDebug>

enum {
    debug = false
};

namespace Core {

struct EditorToolBarPrivate
{
    explicit EditorToolBarPrivate(QWidget *parent, EditorToolBar *q);

    QComboBox *m_editorListMaybe;
	QTabBar*	m_tabBar;
    QToolButton *m_closeEditorButton;
    QToolButton *m_lockButton;
    QToolButton *m_dragHandleMaybe;
    QMenu *m_dragHandleMenu;
    EditorToolBar::MenuProvider m_menuProvider;
    QAction *m_goBackAction;
    QAction *m_goForwardAction;
    QToolButton *m_backButton;
    QToolButton *m_forwardButton;
    QToolButton *m_splitButton;
    QAction *m_horizontalSplitAction;
    QAction *m_verticalSplitAction;
    QAction *m_splitNewWindowAction;
    QToolButton *m_closeSplitButton;

    QWidget *m_activeToolBar;
    QWidget *m_toolBarPlaceholder;
    QWidget *m_defaultToolBar;

    QPoint m_dragStartPosition;

    bool m_isStandalone;
	
};

bool EditorUseTabBar(){return true;}
bool EditorClassicControls(){return !EditorUseTabBar();}


EditorToolBarPrivate::EditorToolBarPrivate(QWidget *parent, EditorToolBar *q) :
    m_editorListMaybe(EditorClassicControls()?new QComboBox(q):nullptr),
    m_closeEditorButton(new QToolButton(q)),
    m_lockButton(new QToolButton(q)),
    m_dragHandleMaybe(EditorClassicControls()?new QToolButton(q):nullptr),
    m_dragHandleMenu(0),
    m_goBackAction(new QAction(Utils::Icons::PREV_TOOLBAR.icon(), EditorManager::tr("Go Back"), parent)),
    m_goForwardAction(new QAction(Utils::Icons::NEXT_TOOLBAR.icon(), EditorManager::tr("Go Forward"), parent)),
    m_backButton(new QToolButton(q)),
    m_forwardButton(new QToolButton(q)),
    m_splitButton(new QToolButton(q)),
    m_horizontalSplitAction(new QAction(Utils::Icons::SPLIT_HORIZONTAL.icon(),
                                        EditorManager::tr("Split"), parent)),
    m_verticalSplitAction(new QAction(Utils::Icons::SPLIT_VERTICAL.icon(),
                                      EditorManager::tr("Split Side by Side"), parent)),
    m_splitNewWindowAction(new QAction(EditorManager::tr("Open in New Window"), parent)),
    m_closeSplitButton(new QToolButton(q)),
    m_activeToolBar(0),
    m_toolBarPlaceholder(new QWidget(q)),
    m_defaultToolBar(new QWidget(q)),
    m_isStandalone(false)
{
	if (EditorUseTabBar){
		auto tb= m_tabBar=new QTabBar(m_defaultToolBar);
		tb->setDocumentMode(true);
		tb->setMovable(true);
		tb->setShape(QTabBar::RoundedNorth);
		tb->setDrawBase(false);	
		tb->setUsesScrollButtons(true);
		tb->setTabsClosable(true);
	}
	

}

/*!
  Mimic the look of the text editor toolbar as defined in e.g. EditorView::EditorView
  */

template<typename T,typename Y>
void addWidgetMaybe(T* dst,Y* src){
	if (src && dst) dst->addWidget(src);
}
template<typename T,typename Y>
void addWidgetMaybe(T* dst,Y* src,int i){
	if (src && dst) dst->addWidget(src,i);
}
EditorToolBar::EditorToolBar(QWidget *parent) :
        Utils::StyledBar(parent), d(new EditorToolBarPrivate(parent, this))
{
    QHBoxLayout *toolBarLayout = new QHBoxLayout(this);
    toolBarLayout->setMargin(0);
    toolBarLayout->setSpacing(0);
    toolBarLayout->addWidget(d->m_defaultToolBar);
	d->m_toolBarPlaceholder->setLayout(toolBarLayout);
	d->m_toolBarPlaceholder->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	

    d->m_defaultToolBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    d->m_activeToolBar = d->m_defaultToolBar;

    d->m_lockButton->setAutoRaise(true);
    d->m_lockButton->setEnabled(false);

	if (auto dh=d->m_dragHandleMaybe){
	    dh->setProperty("noArrow", true);
	    dh->setToolTip(tr("Drag to drag documents between splits"));
	    dh->installEventFilter(this);
	    d->m_dragHandleMenu = new QMenu(dh);
        dh->setMenu(d->m_dragHandleMenu);
	}
    connect(d->m_goBackAction, &QAction::triggered, this, &EditorToolBar::goBackClicked);
    connect(d->m_goForwardAction, &QAction::triggered, this, &EditorToolBar::goForwardClicked);
	if (d->m_tabBar)
		connect(d->m_tabBar,&QTabBar::currentChanged, this,&EditorToolBar::changeTab);

	if (auto el=d->m_editorListMaybe){
	    el->setProperty("hideicon", true);
		el->setProperty("notelideasterisk", true);
		el->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		el->setMinimumContentsLength(20);
		el->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		el->setModel(DocumentModel::model());
	    el->setMaxVisibleItems(40);
		el->setContextMenuPolicy(Qt::CustomContextMenu);
	}
	if (d->m_closeEditorButton){
		d->m_closeEditorButton->setAutoRaise(true);
		d->m_closeEditorButton->setIcon(Utils::Icons::CLOSE_TOOLBAR.icon());
		d->m_closeEditorButton->setEnabled(false);
		d->m_closeEditorButton->setProperty("showborder", true);
	}
	d->m_toolBarPlaceholder->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    d->m_backButton->setDefaultAction(d->m_goBackAction);

    d->m_forwardButton->setDefaultAction(d->m_goForwardAction);

    d->m_splitButton->setIcon(Utils::Icons::SPLIT_HORIZONTAL_TOOLBAR.icon());
    d->m_splitButton->setToolTip(tr("Split"));
    d->m_splitButton->setPopupMode(QToolButton::InstantPopup);
    d->m_splitButton->setProperty("noArrow", true);
    QMenu *splitMenu = new QMenu(d->m_splitButton);
    splitMenu->addAction(d->m_horizontalSplitAction);
    splitMenu->addAction(d->m_verticalSplitAction);
    splitMenu->addAction(d->m_splitNewWindowAction);
    d->m_splitButton->setMenu(splitMenu);

    d->m_closeSplitButton->setAutoRaise(true);
    d->m_closeSplitButton->setIcon(Utils::Icons::CLOSE_SPLIT_BOTTOM.icon());

    QHBoxLayout *toplayout = new QHBoxLayout(this);
    toplayout->setSpacing(0);
    toplayout->setMargin(0);
    addWidgetMaybe(toplayout,d->m_backButton);
    addWidgetMaybe(toplayout,d->m_forwardButton);
	addWidgetMaybe(toplayout,d->m_tabBar);
    addWidgetMaybe(toplayout,d->m_lockButton);
    addWidgetMaybe(toplayout,d->m_dragHandleMaybe);
    addWidgetMaybe(toplayout,d->m_editorListMaybe);
    addWidgetMaybe(toplayout,d->m_closeEditorButton);
    addWidgetMaybe(toplayout,d->m_toolBarPlaceholder, 1); // Custom toolbar stretches
    addWidgetMaybe(toplayout,d->m_splitButton);
    addWidgetMaybe(toplayout,d->m_closeSplitButton);

    setLayout(toplayout);

    // this signal is disconnected for standalone toolbars and replaced with
    // a private slot connection
	if (auto el=d->m_editorListMaybe){
	    connect(el, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            this, &EditorToolBar::listSelectionActivated);

		connect(el, &QComboBox::customContextMenuRequested, [this](QPoint p) {
       QMenu menu;
       fillListContextMenu(&menu);
       menu.exec(d->m_editorListMaybe->mapToGlobal(p));
    });
	};
    connect(d->m_dragHandleMenu, &QMenu::aboutToShow, [this]() {
       d->m_dragHandleMenu->clear();
       fillListContextMenu(d->m_dragHandleMenu);
    });
    connect(d->m_lockButton, &QAbstractButton::clicked, this, &EditorToolBar::makeEditorWritable);
    connect(d->m_closeEditorButton, &QAbstractButton::clicked,
            this, &EditorToolBar::closeEditor, Qt::QueuedConnection);
    connect(d->m_horizontalSplitAction, &QAction::triggered,
            this, &EditorToolBar::horizontalSplitClicked, Qt::QueuedConnection);
    connect(d->m_verticalSplitAction, &QAction::triggered,
            this, &EditorToolBar::verticalSplitClicked, Qt::QueuedConnection);
    connect(d->m_splitNewWindowAction, &QAction::triggered,
            this, &EditorToolBar::splitNewWindowClicked, Qt::QueuedConnection);
    connect(d->m_closeSplitButton, &QAbstractButton::clicked,
            this, &EditorToolBar::closeSplitClicked, Qt::QueuedConnection);


    connect(ActionManager::command(Constants::CLOSE), &Command::keySequenceChanged,
            this, &EditorToolBar::updateActionShortcuts);
    connect(ActionManager::command(Constants::GO_BACK), &Command::keySequenceChanged,
            this, &EditorToolBar::updateActionShortcuts);
    connect(ActionManager::command(Constants::GO_FORWARD), &Command::keySequenceChanged,
            this, &EditorToolBar::updateActionShortcuts);

    updateActionShortcuts();
}

EditorToolBar::~EditorToolBar()
{
    delete d;
}

void EditorToolBar::removeToolbarForEditor(IEditor *editor)
{
    QTC_ASSERT(editor, return);
    disconnect(editor->document(), &IDocument::changed, this, &EditorToolBar::checkDocumentStatus);

    QWidget *toolBar = editor->toolBar();
    if (toolBar != 0) {
        if (d->m_activeToolBar == toolBar) {
            d->m_activeToolBar = d->m_defaultToolBar;
            d->m_activeToolBar->setVisible(true);
        }
	    d->m_toolBarPlaceholder->layout()->removeWidget(toolBar);
		
        toolBar->setVisible(false);
        toolBar->setParent(0);
    }
}

void EditorToolBar::setCloseSplitEnabled(bool enable)
{
    d->m_closeSplitButton->setVisible(enable);
}

void EditorToolBar::setCloseSplitIcon(const QIcon &icon)
{
    d->m_closeSplitButton->setIcon(icon);
}

void EditorToolBar::closeEditor()
{
    if (d->m_isStandalone)
        EditorManager::slotCloseCurrentEditorOrDocument();
    emit closeClicked();
}

void EditorToolBar::addEditor(IEditor *editor)
{
    QTC_ASSERT(editor, return);
    connect(editor->document(), &IDocument::changed, this, &EditorToolBar::checkDocumentStatus);
    QWidget *toolBar = editor->toolBar();

    if (toolBar && !d->m_isStandalone)
        addCenterToolBar(toolBar);
	if (d->m_tabBar){
		d->m_tabBar->addTab(editor->document()->displayName());
	}

    updateDocumentStatus(editor->document());
}

void EditorToolBar::addCenterToolBar(QWidget *toolBar)
{
    QTC_ASSERT(toolBar, return);
    toolBar->setVisible(false); // will be made visible in setCurrentEditor
	d->m_toolBarPlaceholder->layout()->addWidget(toolBar);

    updateToolBar(toolBar);
}

void EditorToolBar::updateToolBar(QWidget *toolBar)
{
    if (!toolBar)
        toolBar = d->m_defaultToolBar;
    if (d->m_activeToolBar == toolBar)
        return;
    toolBar->setVisible(true);
    d->m_activeToolBar->setVisible(false);
    d->m_activeToolBar = toolBar;
}

void EditorToolBar::setToolbarCreationFlags(ToolbarCreationFlags flags)
{
    d->m_isStandalone = flags & FlagsStandalone;
    if (d->m_isStandalone) {
        connect(EditorManager::instance(), &EditorManager::currentEditorChanged,
                this, &EditorToolBar::updateEditorListSelection);

		if (d->m_editorListMaybe){
	        disconnect(d->m_editorListMaybe, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
                   this, &EditorToolBar::listSelectionActivated);
		    connect(d->m_editorListMaybe, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
                this, &EditorToolBar::changeActiveEditor);
		}
        d->m_splitButton->setVisible(false);
        d->m_closeSplitButton->setVisible(false);
    }
}

void EditorToolBar::setMenuProvider(const EditorToolBar::MenuProvider &provider)
{
    d->m_menuProvider = provider;
}

void EditorToolBar::setCurrentEditor(IEditor *editor)
{
    IDocument *document = editor ? editor->document() : 0;
	if (auto el=d->m_editorListMaybe){
	    el->setCurrentIndex(DocumentModel::rowOfDocument(document));
	}
	if (d->m_tabBar){
		qDebug()<<"TODO tab bar setCurentEditor goes here\n";
	//	d->m_tabBar->insertTab(editor->document()->filename());
	}
    // If we never added the toolbar from the editor,  we will never change
    // the editor, so there's no need to update the toolbar either.
    if (!d->m_isStandalone)
        updateToolBar(editor ? editor->toolBar() : 0);

    updateDocumentStatus(document);
}

void EditorToolBar::updateEditorListSelection(IEditor *newSelection)
{
    if (newSelection){
		if (d->m_editorListMaybe)
	        d->m_editorListMaybe->setCurrentIndex(DocumentModel::rowOfDocument(newSelection->document()));
		qDebug()<<"TODO tabBar stuff";
	}
}

void EditorToolBar::changeTab(int index){
	qDebug()<<"tabBar: change tab to "<<index;
//	for (int i=0;i<DocumentModel::entryCount(); i++) {
//		if (DocumentModel::entryAtRow(i)->displayName()==d->m_tabBar->tabText(index)){
//		    EditorManager::activateEditorForEntry(DocumentModel::entryAtRow(i));	
//		}
//	}
	auto ls=DocumentModel::entries();
	for (int i=0; i<ls.count(); i++) {
		auto e=ls[i];
		if (e->displayName()==d->m_tabBar->tabText(index)){
		    EditorManager::activateEditorForEntry(e);			
		}
	}
}

void EditorToolBar::changeActiveEditor(int row)
{
	qDebug()<<"TODO tabBar stuff";
	auto ed=DocumentModel::entryAtRow(row);
	if (ed && d->m_tabBar){
		for (int i=0; i<d->m_tabBar->count();i++){
			if (d->m_tabBar->tabText(i)==ed->displayName()){
				d->m_tabBar->setCurrentIndex(i);
			}
		}
	}
    EditorManager::activateEditorForEntry(DocumentModel::entryAtRow(row));
}

void EditorToolBar::fillListContextMenu(QMenu *menu)
{
    if (d->m_menuProvider) {
        d->m_menuProvider(menu);
    } else {
        IEditor *editor = EditorManager::currentEditor();
        DocumentModel::Entry *entry = editor ? DocumentModel::entryForDocument(editor->document())
                                             : 0;
        EditorManager::addSaveAndCloseEditorActions(menu, entry, editor);
        menu->addSeparator();
        EditorManager::addNativeDirAndOpenWithActions(menu, entry);
    }
}

void EditorToolBar::makeEditorWritable()
{
    if (IDocument *current = EditorManager::currentDocument())
        Internal::EditorManagerPrivate::makeFileWritable(current);
}

void EditorToolBar::setCanGoBack(bool canGoBack)
{
    d->m_goBackAction->setEnabled(canGoBack);
}

void EditorToolBar::setCanGoForward(bool canGoForward)
{
    d->m_goForwardAction->setEnabled(canGoForward);
}

void EditorToolBar::updateActionShortcuts()
{
    d->m_closeEditorButton->setToolTip(ActionManager::command(Constants::CLOSE)->stringWithAppendedShortcut(EditorManager::tr("Close Document")));
    d->m_goBackAction->setToolTip(ActionManager::command(Constants::GO_BACK)->action()->toolTip());
    d->m_goForwardAction->setToolTip(ActionManager::command(Constants::GO_FORWARD)->action()->toolTip());
    d->m_closeSplitButton->setToolTip(ActionManager::command(Constants::REMOVE_CURRENT_SPLIT)->stringWithAppendedShortcut(tr("Remove Split")));
}

void EditorToolBar::checkDocumentStatus()
{
    IDocument *document = qobject_cast<IDocument *>(sender());
    QTC_ASSERT(document, return);
	if (d->m_editorListMaybe){
    DocumentModel::Entry *entry = DocumentModel::entryAtRow(
                d->m_editorListMaybe->currentIndex());
	
    if (entry && entry->document && entry->document == document)
        updateDocumentStatus(document);
	}
	qDebug()<<"TODO tabbar";
}

void EditorToolBar::updateDocumentStatus(IDocument *document)
{
    d->m_closeEditorButton->setEnabled(document != 0);
	qDebug()<<"TODO:updateDocumentStatus for tabBar";

    if (!document) {
        d->m_lockButton->setIcon(QIcon());
        d->m_lockButton->setEnabled(false);
        d->m_lockButton->setToolTip(QString());
        if (d->m_dragHandleMaybe){d->m_dragHandleMaybe->setIcon(QIcon());}
		if (d->m_editorListMaybe){
	        d->m_editorListMaybe->setToolTip(QString());
		}
        return;
    }

	if (d->m_editorListMaybe){
	    d->m_editorListMaybe->setCurrentIndex(DocumentModel::rowOfDocument(document));
	}

    if (document->filePath().isEmpty()) {
        d->m_lockButton->setIcon(QIcon());
        d->m_lockButton->setEnabled(false);
        d->m_lockButton->setToolTip(QString());
    } else if (document->isFileReadOnly()) {
        const static QIcon locked = Utils::Icons::LOCKED_TOOLBAR.icon();
        d->m_lockButton->setIcon(locked);
        d->m_lockButton->setEnabled(true);
        d->m_lockButton->setToolTip(tr("Make Writable"));
    } else {
        const static QIcon unlocked = Utils::Icons::UNLOCKED_TOOLBAR.icon();
        d->m_lockButton->setIcon(unlocked);
        d->m_lockButton->setEnabled(false);
        d->m_lockButton->setToolTip(tr("File is writable"));
    }

	if (d->m_dragHandleMaybe){
		if (document->filePath().isEmpty())
			d->m_dragHandleMaybe->setIcon(QIcon());
		else
		    d->m_dragHandleMaybe->setIcon(FileIconProvider::icon(document->filePath().toFileInfo()));
	}

	if (d->m_editorListMaybe){
	    d->m_editorListMaybe->setToolTip(document->filePath().isEmpty()
                                ? document->displayName()
                                : document->filePath().toUserOutput());
	}
}

bool EditorToolBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == d->m_dragHandleMaybe && d->m_dragHandleMaybe) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto me = static_cast<QMouseEvent *>(event);
            if (me->buttons() == Qt::LeftButton)
                d->m_dragStartPosition = me->pos();
            return true; // do not pop up menu already on press
        } else if (event->type() == QEvent::MouseButtonRelease) {
            d->m_dragHandleMaybe->showMenu();
            return true;
        } else if (event->type() == QEvent::MouseMove) {
            auto me = static_cast<QMouseEvent *>(event);
            if (me->buttons() != Qt::LeftButton)
                return Utils::StyledBar::eventFilter(obj, event);
            if ((me->pos() - d->m_dragStartPosition).manhattanLength()
                    < QApplication::startDragDistance())
                return Utils::StyledBar::eventFilter(obj, event);
            DocumentModel::Entry *entry = DocumentModel::entryAtRow(
                        d->m_editorListMaybe->currentIndex());
            if (!entry) // no document
                return Utils::StyledBar::eventFilter(obj, event);
            auto drag = new QDrag(this);
            auto data = new Utils::DropMimeData;
            data->addFile(entry->fileName().toString());
            drag->setMimeData(data);
            Qt::DropAction action = drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
            if (action == Qt::MoveAction)
                emit currentDocumentMoved();
            return true;
        }
    }
    return Utils::StyledBar::eventFilter(obj, event);
}

void EditorToolBar::setNavigationVisible(bool isVisible)
{
    d->m_goBackAction->setVisible(isVisible);
    d->m_goForwardAction->setVisible(isVisible);
    d->m_backButton->setVisible(isVisible);
    d->m_forwardButton->setVisible(isVisible);
}

} // Core
