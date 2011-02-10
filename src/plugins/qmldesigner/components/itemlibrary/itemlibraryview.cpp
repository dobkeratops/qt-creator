#include "itemlibraryview.h"
#include "itemlibrarywidget.h"
#include <import.h>

namespace QmlDesigner {

ItemLibraryView::ItemLibraryView(QObject* parent) : AbstractView(parent), m_widget(new ItemLibraryWidget)
{

}

ItemLibraryView::~ItemLibraryView()
{

}

ItemLibraryWidget *ItemLibraryView::widget()
{
    return m_widget.data();
}

void ItemLibraryView::modelAttached(Model *model)
{
    AbstractView::modelAttached(model);
    m_widget->setModel(model);
    updateImports();
}

void ItemLibraryView::modelAboutToBeDetached(Model *model)
{
    AbstractView::modelAboutToBeDetached(model);
    m_widget->setModel(0);
}

void ItemLibraryView::importsChanged(const QList<Import> &/*addedImports*/, const QList<Import> &/*removedImports*/)
{
    updateImports();
}

void ItemLibraryView::nodeCreated(const ModelNode &)
{

}

void ItemLibraryView::nodeRemoved(const ModelNode &, const NodeAbstractProperty &, PropertyChangeFlags)
{

}

void ItemLibraryView::propertiesRemoved(const QList<AbstractProperty> &)
{

}

void ItemLibraryView::variantPropertiesChanged(const QList<VariantProperty> &, PropertyChangeFlags)
{

}

void ItemLibraryView::bindingPropertiesChanged(const QList<BindingProperty> &, PropertyChangeFlags)
{

}

void ItemLibraryView::nodeAboutToBeRemoved(const ModelNode &)
{

}

void ItemLibraryView::nodeOrderChanged(const NodeListProperty &, const ModelNode &, int)
{

}

void ItemLibraryView::nodeAboutToBeReparented(const ModelNode &, const NodeAbstractProperty &, const NodeAbstractProperty &, AbstractView::PropertyChangeFlags)
{

}

void ItemLibraryView::nodeReparented(const ModelNode &, const NodeAbstractProperty &, const NodeAbstractProperty &, AbstractView::PropertyChangeFlags)
{

}

void ItemLibraryView::rootNodeTypeChanged(const QString &, int , int )
{

}

void ItemLibraryView::nodeIdChanged(const ModelNode &, const QString &, const QString& )
{

}

void ItemLibraryView::propertiesAboutToBeRemoved(const QList<AbstractProperty>& )
{

}

void ItemLibraryView::selectedNodesChanged(const QList<ModelNode> &,
                                  const QList<ModelNode> &)
{

}

void ItemLibraryView::auxiliaryDataChanged(const ModelNode &, const QString &, const QVariant &)
{

}

void ItemLibraryView::scriptFunctionsChanged(const ModelNode &, const QStringList &)
{

}

void ItemLibraryView::instancePropertyChange(const QList<QPair<ModelNode, QString> > &)
{

}

void ItemLibraryView::instancesCompleted(const QVector<ModelNode> &)
{

}

void ItemLibraryView::instanceInformationsChange(const QVector<ModelNode> &/*nodeList*/)
{
}

void ItemLibraryView::instancesRenderImageChanged(const QVector<ModelNode> &/*nodeList*/)
{
}

void ItemLibraryView::instancesPreviewImageChanged(const QVector<ModelNode> &/*nodeList*/)
{
}

void ItemLibraryView::instancesChildrenChanged(const QVector<ModelNode> &/*nodeList*/)
{

}

void ItemLibraryView::rewriterBeginTransaction()
{
}

void ItemLibraryView::rewriterEndTransaction()
{
}

void ItemLibraryView::actualStateChanged(const ModelNode &/*node*/)
{
}

void ItemLibraryView::updateImports()
{
    m_widget->updateModel();
}

} //QmlDesigner
