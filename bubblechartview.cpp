#include "bubblechartview.h"
#include "graph_node.h"
#include "taxnodesignalsender.h"
#include "ui_components/bubblechartproperties.h"
#include "colors.h"
#include "config.h"
#include "main_window.h"

#include <math.h>

#include <QKeyEvent>
#include <QDebug>
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QGraphicsItemGroup>

//=========================================================================
BubbleChartView::BubbleChartView(BlastTaxDataProviders *_dataProviders, QWidget *parent, bool setRank)
    : DataGraphicsView(NULL, parent)
{
    flags |= DGF_BUBBLES | DGF_RANKS;
    config = new BubbleChartParameters();

    if ( setRank && mainWindow->getRank() == TR_NORANK )
        mainWindow->setRank(TR_SPECIES);

    if ( _dataProviders == NULL )
        _dataProviders = new BlastTaxDataProviders();

    taxDataProvider = new ChartDataProvider(_dataProviders, this);

    connect((ChartDataProvider *)taxDataProvider, SIGNAL(taxVisibilityChanged(quint32)), this, SLOT(onTaxVisibilityChanged(quint32)));
    connect((ChartDataProvider *)taxDataProvider, SIGNAL(cacheUpdated()), this, SLOT(onDataChanged()));

    QGraphicsScene *s = new QGraphicsScene(this);
    s->setItemIndexMethod(QGraphicsScene::NoIndex);
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setMinimumSize(400, 200);
    setAlignment(Qt::AlignCenter);
    setScene(s);

    setChartRectSize(800, 800);

    propertiesAction = popupMenu.addAction("Properties...");
    connect(propertiesAction, SIGNAL(triggered(bool)), this, SLOT(showPropertiesDialog()));

    if ( _dataProviders->size() > 0 )
    {
        prepareScene();
        showChart();
    }
    onTaxRankChanged(mainWindow->getRank());
}

//=========================================================================
BubbleChartView::~BubbleChartView()
{
    scene()->removeItem(&gGrid);
    scene()->clear();
}

//=========================================================================
void BubbleChartView::setChartRectSize(int w, int h)
{
    chartRect = QRectF(0, 0, w, h);
    scene()->setSceneRect(-4*MARGIN, getConfig()->showTitle ? -MARGIN : 0, chartRect.width()+4*MARGIN, chartRect.height()+MARGIN+(getConfig()->showTitle ? MARGIN : 0));
}

//=========================================================================
void BubbleChartView::resizeEvent(QResizeEvent *e)
{
    QSize s = e->size();
    QRectF r = scene()->itemsBoundingRect();
    setChartRectSize(s.width()*0.8, r.height()/*s.height()*0.9*/);
    if ( dataProvider() != NULL )
        showChart();
}

//=========================================================================
void BubbleChartView::prepareScene()
{
    QPen peng(Qt::black);
    peng.setWidth(1.5);
    QBrush brushg(Qt::transparent);
    chartRectGI = scene()->addRect(chartRect, peng, brushg);
    gGrid.addToGroup(chartRectGI);
    setHeader(/*chartData.header*/ "Chart header");
    ChartDataProvider *dp = dataProvider();
    if ( dp->data.size() == 0 )
        return;
    QPen pen(Qt::lightGray);
    pen.setWidth(0.05);
    QBrush brush(Qt::transparent);
    quint32 swidth = (int)this->sceneRect().height()*0.8;
    quint32 columnWidth = qMin((quint32)getConfig()->bubbleSize, swidth/dp->providers->count());
    grid.clear();
    for ( int i = 0; i < dp->providers->size(); i++ )
    {
        BlastTaxDataProvider *provider = dp->providers->at(i);
        for ( int j = 0; j < dp->data.count(); j++)
        {
            if ( !dp->data.at(j).checked )
                continue;
            const BlastTaxNodes &btns = dp->data.at(j).tax_nodes;
            BlastTaxNode *node = btns.at(i);
            if ( node != NULL )
            {
                quint32 sum = node->sum();
                if ( sum == 0 )
                    continue;
                CreateGraphNode(node, provider);
            }
        }
        qreal x1 = i*columnWidth;
        QGraphicsRectItem *gridItem  = scene()->addRect(chartRect.x()+x1, chartRect.y(), columnWidth, chartRect.height(), pen, brush);
        grid.append(gridItem);
        gGrid.addToGroup(gridItem);
        scene()->addItem(&gGrid);
        gGrid.setVisible(getConfig()->showGrid);
        // Create horizontal axe labels
        BlastTaxDataProvider *p = dp->providers->at(i);
        QString stxt = p->name;
        if ( stxt.length() > 30 )
        {
            stxt.truncate(27);
            stxt.append("...");
        }
        QGraphicsTextItem *item = scene()->addText(stxt);
        item->setToolTip(p->name);
        item->setRotation(45);
        horizontalLegend.append(item);
    }
    for ( int j = 0; j < dp->data.count(); j++)
    {
        const BlastTaxNodes &btns = dp->data.at(j).tax_nodes;
        QString txt;
        for ( int i = 0 ; i < btns.size(); i++ )
        {
            if ( btns[i] == NULL )
                continue;
            txt = btns[i]->getText();
            break;
        }
        QString stxt = txt;
        if ( stxt.length() > 30 )
        {
            stxt.truncate(27);
            stxt.append("...");
        }
        QGraphicsTextItem *item = scene()->addText(stxt);
        item->installEventFilter(this);
        item->setToolTip(txt);
        item->setVisible(dp->data.at(j).checked);
        verticalLegend.append(item);
    }

    header->setPos(0, 10-MARGIN);
    quint32 tw = header->textWidth();
    header->setTextWidth(qMax(tw, dp->providers->count()*columnWidth));
}

//=========================================================================
void BubbleChartView::showChart(bool forceNodeUpdate)
{
    ChartDataProvider *chartDataProvider = dataProvider();
    if ( chartDataProvider->data.size() == 0 )
        return;
    quint32 sheight = (int)this->size().height()*0.8;
    quint32 swidth = (int)this->sceneRect().height()*0.8;
    quint32 tsize = dataProvider()->visibleTaxNumber();
    quint32 bubbleSize = getConfig()->bubbleSize;
    quint32 maxBubbleSize = tsize == 0 ? 0 : qMin(bubbleSize, sheight/tsize);
    quint32 columnWidth = qMin(bubbleSize, swidth/dataProvider()->providers->count())+getConfig()->horInterval;
    quint32 rowHeight = maxBubbleSize + getConfig()->vertInterval;
    int rnum = 0;
    quint32 column = 0;
    for ( int j = 0; j < chartDataProvider->data.count(); j++)
    {
        const BlastTaxNodes &btns = chartDataProvider->data.at(j).tax_nodes;
        if ( !chartDataProvider->data.at(j).checked )
            continue;
        column = 0;
        // In some corner cases if not all the data providers are correctly restored
        // it is possible that no nodes should be created for the spices
        bool has_node = false;
        for ( int i = 0; i < btns.size(); i++ )
        {
            if ( !chartDataProvider->getBlastTaxDataProviders()->isVisible(i) )
                continue;
            qreal x1 = (column++)*columnWidth;
            BlastTaxNode *node = btns.at(i);
            if ( node == NULL || node->sum() == 0 )
                continue;
            has_node = true;
            ChartGraphNode *gnode = (ChartGraphNode*)node->getGnode();
            Q_ASSERT_X(gnode != NULL, "showChart", "GraphNode must be created here");            
            gnode->setMaxNodeSize(bubbleSize);
            gnode->setPos(chartRect.x()+x1+columnWidth/2, chartRect.y()+(rnum)*rowHeight+rowHeight/2);
            if ( forceNodeUpdate )
                gnode->update();
        }
        if ( has_node )
        {
            verticalLegend[j]->setPos(chartRect.x()-verticalLegend[j]->boundingRect().width(), rnum*rowHeight);
            ++rnum;
        }
    }
    quint32 rectHeight = rnum*rowHeight + maxBubbleSize/2;
    chartRectGI->setRect(chartRect.x(), chartRect.y(), column*columnWidth, rectHeight);
    if ( header->isVisible() && false )
    {
        header->setPos(0, 10-MARGIN);
        header->setTextWidth(column*columnWidth);
    }
    column = 0;
    for ( int i = 0; i < chartDataProvider->providers->size(); i++ )
    {
        if ( !horizontalLegend.at(i)->isVisible() )
            continue;
        qreal x1 = column*columnWidth;
        grid.at(i)->setRect(chartRect.x()+x1, chartRect.y(), columnWidth, rectHeight);
        horizontalLegend.at(i)->setPos(chartRect.x()+x1+columnWidth/2, chartRect.y()+rectHeight+5);
        column++;
    }
}


//=========================================================================
void BubbleChartView::setVerticalLegendSelected(qint32 index, bool selected)
{
    if ( index >= 0 )
    {
        verticalLegend[index]->setDefaultTextColor(selected ? Qt::red : Qt::black);
        verticalLegend[index]->update();
    }
}

//=========================================================================
void BubbleChartView::setVerticalLegentColor(BaseTaxNode *node, bool selected)
{
    qint32 index = node == NULL ? -1 : dataProvider()->indexOf(node->getId());
    setVerticalLegendSelected(index, selected);
}

//=========================================================================
void BubbleChartView::compareNodesAndUpdate(ChartGraphNode *chartGraphNode, BaseTaxNode *refNode)
{
    if ( refNode != NULL )
    {
        if ( chartGraphNode->getTaxNode()->getId() == refNode->getId() )
            chartGraphNode->update();
    }
}

//=========================================================================
void BubbleChartView::onCurrentNodeChanged(BaseTaxNode *node)
{
    BaseTaxNode *curNode = currentNode();
    if ( curNode == node )
        return;
    BaseTaxNode *oldCurNode = curNode;
    taxDataProvider->current_tax_id = node->getId();
    foreach(QGraphicsItem *item, scene()->items())
    {
        if ( item->type() == GraphNode::Type )
        {
            ChartGraphNode *n = (ChartGraphNode *)item;
            compareNodesAndUpdate(n, oldCurNode);
            compareNodesAndUpdate(n, node);
        }
    }
    if ( curNode != NULL && oldCurNode != NULL && node->getId() == oldCurNode->getId() )
        return;
    setVerticalLegentColor(node, true);
    setVerticalLegentColor(oldCurNode, false);
}

//=========================================================================
void BubbleChartView::CreateGraphNode(BlastTaxNode *node, BlastTaxDataProvider *bProvider)
{
    ChartGraphNode *gnode = new ChartGraphNode(this, node, bProvider);
    node->gnode = gnode;
    gnode->updateToolTip();
    scene()->addItem(gnode);
}

//=========================================================================
void BubbleChartView::toJson(QJsonObject &json) const
{
    json["Type"] = QString("ChartView");
    json["Header"] = header->toPlainText();
    QJsonObject jConfig;
    getConfig()->toJson(jConfig);
    json["Config"] = jConfig;
    QJsonObject jDp;
    dataProvider()->toJson(jDp);
    json["Dp"] = jDp;
}

//=========================================================================
void BubbleChartView::fromJson(QJsonObject &json)
{
    try
    {
        QJsonObject jDp = json["Dp"].toObject();
        dataProvider()->fromJson(jDp);
        QJsonObject jConfig = json["Config"].toObject();
        getConfig()->fromJson(jConfig);
        prepareScene();
        header->setPlainText(json["Header"].toString());
        showChart();
    }
    catch (...)
    {
        QMessageBox::warning(this, "Error occured", "Cannot restore Bubble Chart from project configuration");
    }
}

//=========================================================================
void BubbleChartView::onTaxVisibilityChanged(quint32 index)
{
    ChartDataProvider *chartDataProvider = dataProvider();
    const BlastTaxNodes &btns = chartDataProvider->data.at(index).tax_nodes;
    bool createGNode = chartDataProvider->data.at(index).checked;
    for ( int i = 0; i < btns.size(); i++ )
    {
        BlastTaxDataProvider *bProvider = chartDataProvider->getBlastTaxDataProviders()->at(i);
        BlastTaxNode *node = btns.at(i);
        if ( node == NULL )
            continue;
        if ( createGNode )
        {
            CreateGraphNode(node, bProvider);
        }
        else
        {
            ChartGraphNode *gnode = (ChartGraphNode *) node->getGnode();
            if ( gnode != NULL )
                delete gnode;
        }
    }
    verticalLegend[index]->setVisible(chartDataProvider->data.at(index).checked);
    showChart(true);
}

//=========================================================================
void BubbleChartView::onDataSourceVisibilityChanged(int index)
{
    ChartDataProvider *dp = dataProvider();
    BlastTaxDataProviders *btdps = dp->getBlastTaxDataProviders();
    BlastTaxDataProvider *bProvider = btdps->at(index);
    bool visible = btdps->isVisible(index);
    for ( int i = 0; i < dp->data.count(); i++ )
    {
        if ( !dp->data.at(i).checked )
            continue;
        BlastTaxNode *node = dp->data.at(i).tax_nodes[index];
        if ( node == NULL )
            continue;
        GraphNode *gnode = node->getGnode();
        if ( visible )
        {
            if ( gnode == NULL )
                CreateGraphNode(node, bProvider);
        }
        else
        {
            if ( gnode != NULL )
                delete gnode;
        }
    }
    horizontalLegend.at(index)->setVisible(visible);
    grid.at(index)->setVisible(visible);
    showChart(true);
}

//=========================================================================
void BubbleChartView::onDataChanged()
{
    for ( int j = 0; j < verticalLegend.size(); j++)
    {
        if ( !verticalLegend[j]->toPlainText().isEmpty() )
            continue;
        const BlastTaxNodes &btns = dataProvider()->data.at(j).tax_nodes;
        for ( int i = 0 ; i < btns.size(); i++ )
        {
            if ( btns[i] == NULL )
                continue;
            QString txt = btns[i]->getText();
            QString stxt = txt;
            if ( stxt.length() > 30 )
            {
                stxt.truncate(27);
                stxt.append("...");
            }
            verticalLegend[j]->setPlainText(stxt);
            verticalLegend[j]->setToolTip(txt);
            QPointF pos = verticalLegend[j]->pos();
            pos.setX(pos.x() - verticalLegend[j]->boundingRect().width());
            verticalLegend[j]->setPos(pos);
            verticalLegend[j]->update();
            break;
        }
    }
}

//=========================================================================
void BubbleChartView::showContextMenu(const QPoint &pos)
{
    QGraphicsItem *item = scene()->itemAt(mapToScene(pos), QTransform());
    ChartGraphNode *cgn = dynamic_cast<ChartGraphNode *>(item);
    QMenu *newMenu = new QMenu(this);
    if ( cgn != NULL )
    {
        QAction *hideCurTax = newMenu->addAction("Hide");
        connect(hideCurTax, SIGNAL(triggered(bool)), this, SLOT(hideCurrentTax()));
        QAction *setCurrentTaxColor = newMenu->addAction("Color...");
        connect(setCurrentTaxColor, SIGNAL(triggered(bool)), this, SLOT(setCurrentTaxColor()));
    }
    else
    {
        QGraphicsTextItem *gti = dynamic_cast<QGraphicsTextItem *>(item);
        for ( int i = 0; i < verticalLegend.count(); i++ )
        {
            if ( gti == verticalLegend[i] )
            {
                TaxNodeSignalSender *tnss = getTaxNodeSignalSender(dataProvider()->taxNode(i));
                tnss->makeCurrent();
                QAction *hideCurTax = newMenu->addAction("Hide");
                connect(hideCurTax, SIGNAL(triggered(bool)), this, SLOT(hideCurrentTax()));
                break;
            }
        }
    }
    newMenu->addActions(popupMenu.actions());
    newMenu->exec(mapToGlobal(pos));
}

//=========================================================================
void BubbleChartView::update()
{
    showChart();
  /*  qreal newHeight = scene()->itemsBoundingRect().height();
    QRectF r = sceneRect();
    r.setHeight(newHeight);
    setSceneRect(r);*/
    QRectF rect = sceneRect();
    QRectF ibr = scene()->itemsBoundingRect();
    rect.setHeight(ibr.height()+60);
    qreal sizew = size().width()*0.8;
    rect.setWidth(/*ibr.width() > sizew ? ibr.width() :*/ sizew);
    setSceneRect(rect);
}

//=========================================================================
void BubbleChartView::updateForce()
{
    showChart(true);
}

//=========================================================================
void BubbleChartView::onBubbleSizeChanged(quint32, quint32 newS)
{
    getConfig()->bubbleSize = newS;
    showChart();
}

//=========================================================================
void BubbleChartView::toggleTitleVisibility(bool visible)
{
    QSize s = size();
    header->setVisible(visible);
    setChartRectSize(s.width()*0.8, s.height()*0.8);
    showChart();
}

//=========================================================================
void BubbleChartView::onNormalizedChanged(bool)
{

}

//=========================================================================
void BubbleChartView::setHeader(QString fileName)
{
    int idx = fileName.lastIndexOf('/');
    fileName = fileName.mid(idx+1);
    idx = fileName.indexOf('.');
    fileName = fileName.left(idx);
    header = scene()->addText(fileName);
    header->document()->setDefaultTextOption(QTextOption(Qt::AlignCenter | Qt::AlignVCenter));
    header->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsMovable);
    header->setTextInteractionFlags(Qt::TextEditorInteraction);
    QFont font;
    font.setPixelSize(25);
    font.setBold(true);
    QFontMetricsF fm(font);
    header->setTextWidth(fm.width(fileName)+10);
    header->setFont(font);
    header->setVisible(getConfig()->showTitle);
}


//=========================================================================
ChartGraphNode *BubbleChartView::getGNode(BlastTaxNode *node)
{
    foreach(QGraphicsItem *item, scene()->items())
    {
        if ( item->type() == GraphNode::Type )
        {
            ChartGraphNode *n = (ChartGraphNode *)item;
            if ( n->getTaxNode() == node )
                return n;
        }
    }
    return NULL;
}

//=========================================================================
void BubbleChartView::goUp()
{
    BaseTaxNode *cNode = currentNode();
    if ( cNode == NULL )
        return;
    qint32 index = dataProvider()->indexOf(cNode->getId());
    if ( index <= 0 )
        return;
    TaxNodeSignalSender *tnss = getTaxNodeSignalSender(dataProvider()->taxNode(--index));
    tnss->makeCurrent();
}

//=========================================================================
void BubbleChartView::goDown()
{
    BaseTaxNode *cNode = currentNode();
    if ( cNode == NULL )
        return;
    quint32 index = dataProvider()->indexOf(cNode->getId());
    if ( ++index == dataProvider()->count() )
        return;
    TaxNodeSignalSender *tnss = getTaxNodeSignalSender(dataProvider()->taxNode(index));
    tnss->makeCurrent();
}

//=========================================================================
void BubbleChartView::showPropertiesDialog()
{
    BubbleChartProperties *propertiesDialog = new BubbleChartProperties(this, getConfig(), dataProvider()->getBlastTaxDataProviders());
    connect(propertiesDialog, SIGNAL(maxBubbleSizeChanged(int)), this, SLOT(update()));
    connect(propertiesDialog, SIGNAL(showTitleToggled(bool)), this, SLOT(toggleTitleVisibility(bool)));
    connect(propertiesDialog, SIGNAL(dataSourceVisibilityChanged(int)), this, SLOT(onDataSourceVisibilityChanged(int)));
    connect(propertiesDialog, SIGNAL(bubbleSizeCalcMethodChanged(int)), this, SLOT(updateForce()));
    connect(propertiesDialog, SIGNAL(dataSourceMoved(int, int)), this, SLOT(onDataSourceMoved(int, int)));
    connect(propertiesDialog, SIGNAL(normalizedChanged(bool)), this, SLOT(updateForce()));
    connect(propertiesDialog, SIGNAL(showGridChanged(bool)), this, SLOT(onShowGridChanged(bool)));
    connect(propertiesDialog, SIGNAL(horIntervalChanged(int)), this, SLOT(update()));
    connect(propertiesDialog, SIGNAL(vertIntervalChanged(int)), this, SLOT(update()));
    connect(propertiesDialog, SIGNAL(bubbleTransparancyChanged(int)), this, SLOT(updateForce()));
    propertiesDialog->show();
}

//=========================================================================
void BubbleChartView::showDataSourceDialog()
{
}

//=========================================================================
void BubbleChartView::onColorChanged(BaseTaxNode *node)
{
    quint32 index = dataProvider()->indexOf(node->getId());
    foreach(BlastTaxNode *btn, dataProvider()->data.at(index).tax_nodes)
    {
        if ( btn == NULL )
            continue;
        GraphNode *gn = btn->getGnode();
        if ( gn != NULL )
            gn->update();
    }
}

//=========================================================================
void BubbleChartView::onTaxRankChanged(TaxRank rank)
{
    ChartDataProvider *dp = dataProvider();
    int c = dp->count();
    int max = configuration->BubbleChart()->defaultVisibleChartTaxes();
    for ( int i = c-1; i >= 0; --i )
    {
        BaseTaxNode *node = dp->taxNode(i);
        TaxNode *tn = taxMap.value(node->getId());
        bool checked = max > 0 && tn->getRank() == rank;
        dp->setCheckedState(i, checked ? Qt::Checked : Qt::Unchecked);
        if ( checked )
            --max;
    }
}

//=========================================================================
void BubbleChartView::onDataSourceMoved(int index, int dest)
{
    horizontalLegend.move(index, dest);
    ChartDataProvider *chartDataProvider = dataProvider();
    for ( quint32 i = 0 ; i < chartDataProvider->count(); i++ )
        chartDataProvider->data[i].tax_nodes.move(index, dest);
    showChart();
}

//=========================================================================
void BubbleChartView::onShowGridChanged(bool val)
{
    gGrid.setVisible(val);
}

//=========================================================================
void BubbleChartView::hideCurrentTax()
{
    TaxNodeSignalSender *tnss = getTaxNodeSignalSender(currentNode());
    tnss->VisibilityChanged(false);
}

//=========================================================================
void BubbleChartView::setCurrentTaxColor()
{
    colors->pickColor(taxDataProvider->current_tax_id);
}

//=========================================================================
void BubbleChartView::keyPressEvent(QKeyEvent *event)
{
    switch( event->key() )
    {
        case Qt::Key_Up:
            goUp();
            return;
        case Qt::Key_Down:
            goDown();
            return;
    }
    QGraphicsView::keyPressEvent(event);
}

//=========================================================================
bool BubbleChartView::eventFilter(QObject *object, QEvent *event)
{
    if ( event->type() == QEvent::GraphicsSceneMousePress )
    if ( object->metaObject() == &QGraphicsTextItem::staticMetaObject )
    {
        QGraphicsTextItem *item = (QGraphicsTextItem*)object;
        qint32 index = verticalLegend.indexOf(item);
        if ( index >= 0 )
        {
            TaxNodeSignalSender *tnss = getTaxNodeSignalSender(dataProvider()->taxNode(index));
            tnss->makeCurrent();
        }
    }
    return DataGraphicsView::eventFilter(object, event);
}

//=========================================================================
//************************************************************************
//=========================================================================
//=========================================================================
ChartDataProvider::ChartDataProvider(BlastTaxDataProviders *_providers, QObject *parent)
    : TaxDataProvider(parent),
      providers(_providers)
{
    name = "Bubble chart";
    addParentToAllDataProviders();
    updateCache(false);
}

//=========================================================================
ChartDataProvider::~ChartDataProvider()
{
    foreach(BlastTaxDataProvider *p, *providers)
        p->removeParent();
    for ( int i = 0; i < data.count(); i++ )
    {
        BlastTaxNodes &nodes = data[i].tax_nodes;
        for ( int j = 0; j < nodes.count(); j++ )
            delete nodes[j];
    }
}

//=========================================================================
void ChartDataProvider::addParentToAllDataProviders()
{
    for ( int i = 0; i < providers->size(); i++ )
        providers->at(i)->addParent();
}

//=========================================================================
quint32 ChartDataProvider::count()
{
    return data.count();
}

//=========================================================================
QString ChartDataProvider::text(quint32 index)
{
    foreach (BlastTaxNode *tn, data[index].tax_nodes)
    {
        if ( tn != NULL )
            return tn->tNode->getText();
    }
    return QString();
}

//=========================================================================
qint32 ChartDataProvider::id(quint32 index)
{
    foreach (BlastTaxNode *tn, data[index].tax_nodes)
    {
        if ( tn != NULL )
            return tn->getId();
    }
    return 0;
}

//=========================================================================
quint32 ChartDataProvider::reads(quint32 index)
{
    quint32 res = 0;
    foreach (BlastTaxNode *tn, data[index].tax_nodes)
        if ( tn != NULL )
            res += tn->reads;
    return res;
}

//=========================================================================
quint32 ChartDataProvider::readsById(quint32 id)
{
    quint32 index = indexOf(id);
    return reads(index);
}

//=========================================================================
quint32 ChartDataProvider::sum(quint32 index)
{
    quint32 sum = 0;
    foreach (BlastTaxNode *tn, data[index].tax_nodes)
        if ( tn != NULL )
            sum += tn->sum();
    return sum;
}

//=========================================================================
static ChartDataProvider *cdp;
bool sumLessThan(const IdBlastTaxNodesPair &x1, const IdBlastTaxNodesPair &x2)
{
    return x1.sum() < x2.sum();
}

//=========================================================================
void ChartDataProvider::updateCache(bool ids_only)
{
    if ( !ids_only )
    {
        maxreads = 0;
        for ( int i = 0; i < providers->size(); i++ )
        {
            BlastTaxDataProvider *p = providers->at(i);
            for ( quint32 j = 0; j < p->count(); j++ )
            {
                if ( p->checkState(j) == Qt::Checked )
                {
                    quint32 taxid = p->id(j);
                    if ( taxMap.value(taxid)->getRank() <= 0 )
                        continue;
                    quint32 sum = p->sum(j);
                    if ( sum == 0 )
                        continue;                    
                    qint32 ind = indexOf(taxid);
                    if ( ind == -1 )
                        data.append(IdBlastTaxNodesPair(taxid));
                    else
                        data[ind].tax_nodes.clear();
                    if ( sum > maxreads )
                        maxreads = sum;
                }
            }
        }

        maxTotalReads = 0;

        for ( int i = 0; i < providers->size(); i++ )
        {
            BlastTaxDataProvider *p = providers->at(i);
            if ( p->totalReads > maxTotalReads )
                maxTotalReads = p->totalReads;
            for ( int j = 0; j < data.count(); j++)
            {
                int id = data[j].id;
                qint32 index = p->indexOf(id);
                if ( index >= 0 )
                    data[j].tax_nodes.append(((BlastTaxNode *)p->taxNode(index))->clone());
                else
                    data[j].tax_nodes.append(NULL);
            }
        }

        cdp = this;
        qSort(data.begin(), data.end(), sumLessThan);
        while ( data.size() > configuration->BubbleChart()->maxChartTaxes() )
            data.removeFirst();
        quint32 dsize = data.size();
        for ( quint32 i = configuration->BubbleChart()->defaultVisibleChartTaxes(); i < dsize; i++ )
            data[dsize-i-1].checked = false;
    }
    emit cacheUpdated();
}

//=========================================================================
QColor ChartDataProvider::color(int index)
{
    return colors->getColor(data[index].id);
}

//=========================================================================
void ChartDataProvider::sort(int /*column*/, Qt::SortOrder /*order*/)
{
    // TODO:
}

//=========================================================================
quint32 ChartDataProvider::getMaxReads()
{
    return maxreads;
}

//=========================================================================
bool ChartDataProvider::contains(quint32 id)
{
    for( int i = 0; i < data.size(); i++ )
        if ( data[i].id == id )
            return true;
    return false;
}

//=========================================================================
qint32 ChartDataProvider::indexOf(qint32 id)
{
    for( int i = 0; i < data.size(); i++ )
        if ( data[i].id == (quint32)id )
            return i;
    return -1;
}

//=========================================================================
QVariant ChartDataProvider::checkState(int index)
{
    return data[index].checked ? Qt::Checked : Qt::Unchecked;
}

//=========================================================================
void ChartDataProvider::setCheckedState(int index, QVariant val)
{
    bool checked = val == Qt::Checked;
    if ( checked == data[index].checked )
        return;
    data[index].checked = checked;
    maxreads = 0;
    for ( qint32 i = 0; i < data.count(); i++ )
    {
        IdBlastTaxNodesPair &di = data[i];
        if ( !di.checked )
            continue;
        for ( int j = 0; j < di.tax_nodes.size(); j++ )
        {
            BlastTaxNode *node = di.tax_nodes[j];
            if ( node == NULL )
                continue;
            quint32 reads = node->sum();
            if ( reads > maxreads )
                 maxreads = reads;
        }
    }
    emit taxVisibilityChanged(index);
}

//=========================================================================
quint32 ChartDataProvider::visibleTaxNumber()
{
    quint32 vbnum = 0;
    for ( int i = 0 ; i  < data.size(); i++ )
        if ( data.at(i).checked )
            vbnum++;
    return vbnum;
}

//=========================================================================
void ChartDataProvider::toJson(QJsonObject &json)
{
    try
    {
        json["maxreads"] = (qint64)maxreads;
        QJsonArray providersArray;
        for ( int i = 0 ; i < providers->size(); i++ )
            providersArray.append(providers->at(i)->name);
        json["BlastDataProviders"] = providersArray;
        QJsonArray tax_array;
        for ( int i = 0; i < data.size(); i++ )
        {
            QJsonArray tax_node;
            tax_node.append((qint64)data[i].id);
            tax_node.append(data[i].checked ? 1: 0);
            tax_array.append(tax_node);
        }
        json["tax_ids"] = tax_array;
    }
    catch (...)
    {
        QMessageBox::warning(NULL, "Error occured", "Cannot restore data needed for chart");
    }
}

//=========================================================================
void ChartDataProvider::fromJson(QJsonObject &json)
{
    maxreads = json["maxreads"].toInt();
    QJsonArray providersArray = json["BlastDataProviders"].toArray();
    for ( int i = 0; i < providersArray.count(); i++ )
    {
        QString pName = providersArray[i].toString();
        BlastTaxDataProvider *p = blastTaxDataProviders.providerByName(pName);
        if ( p == NULL )
            continue;
        providers->addProvider(p);
        p->addParent();
    }
    QJsonArray tax_array = json["tax_ids"].toArray();
    data.reserve(tax_array.count());
    for ( int i = 0 ; i < tax_array.count(); i++ )
    {
        QJsonArray tax_node = tax_array[i].toArray();
        quint32 tax_id = (quint32)tax_node[0].toInt();
        bool checked = (quint32)tax_node[1].toInt() == 1;
        data.append(IdBlastTaxNodesPair(tax_id, checked));
    }
    maxTotalReads = 0;
    for ( int i = 0; i < providers->size(); i++ )
    {
        BlastTaxDataProvider *p = providers->at(i);
        if ( maxTotalReads < p->totalReads )
            maxTotalReads = p->totalReads;
        for ( int j = 0; j < data.count(); j++)
        {
            int id = data[j].id;
            qint32 index = p->indexOf(id);
            quint32 reads = p->reads(id);
            if ( reads > maxreads)
                maxreads = reads;
            data[j].tax_nodes.append( index < 0 ? NULL : ((BlastTaxNode *)p->taxNode(index))->clone());
        }
    }
    updateCache(true);
}

//=========================================================================
BaseTaxNode *ChartDataProvider::taxNode(qint32 index)
{
    if ( index < 0 )
        return NULL;
    BlastTaxNodes btns = data[index].tax_nodes;
    for ( int i = 0; i < btns.count(); i++ )
    {
        BlastTaxNode *btn = btns[i];
        if ( btn != NULL )
            return btn->tNode;
    }
    return NULL;
}

//=========================================================================
//*************************************************************************
//=========================================================================
quint32 IdBlastTaxNodesPair::sum() const
{
    quint32 res = 0;
    foreach (BlastTaxNode *n, tax_nodes )
        if ( n != NULL )
            res += n->sum();
    return res;
}

//=========================================================================
//*************************************************************************
//=========================================================================
void BubbleChartParameters::toJson(QJsonObject &jConf)
{
    BubbledGraphViewConfig::toJson(jConf);
    jConf["showTitle"] = showTitle;
    jConf["normalized"] = normalized;
    jConf["horInterval"] = horInterval;
    jConf["vertInterval"] = vertInterval;
    jConf["bubbleTransparancy"] = bubbleTransparancy;
    jConf["showGrid"] = showGrid;
}

//=========================================================================
void BubbleChartParameters::fromJson(QJsonObject &jConf)
{
    BubbledGraphViewConfig::fromJson(jConf);
    showTitle = jConf["showTitle"].toBool();
    normalized = jConf["normalized"].toBool();
    horInterval = jConf["horInterval"].toInt();
    vertInterval = jConf["vertInterval"].toInt();
    bubbleTransparancy = jConf["bubbleTransparancy"].toInt();
    showGrid = jConf["showGrid"].toBool();
}
