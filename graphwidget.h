/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QGraphicsView>
#include "tax_map.h"
#include "threadsafelist.h"

class GraphNode;
class TreeLoaderThread;
class TaxColorSrc;
class BlastData;

class DirtyGNodesList: public ThreadSafeList<GraphNode *>
{
public:
    virtual void Add(GraphNode *node);
};

class GraphView : public QGraphicsView
{
    Q_OBJECT

public:
    GraphView(QWidget *parent, TaxNode *taxTree);

    int max_node_y;
    BaseTaxNode *root;
    TaxMap *tax_map;
    void reset();
    void adjust_scene_boundaries();
    inline void set_vert_interval(int interval) { vert_interval = interval; }
    inline int get_vert_interval() const { return vert_interval; }
    virtual void resizeEvent(QResizeEvent *e);
    void resetNodesCoordinates();
    void generateTestNodes();
    void generateDefaultNodes();
    void AddNodeToScene(BaseTaxNode *node);
    void CreateGraphNode(BaseTaxNode *node);
    void adjustAllEdges();
    void markAllNodesDirty();
    void updateDirtyNodes(quint32 flag);
    void createMissedGraphNodes();

    DirtyGNodesList dirtyList;

protected:
#ifndef QT_NO_WHEELEVENT
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
#endif
private:
    int hor_interval;
    int vert_interval;
    bool create_nodes;

    void shrink_vertically(int s=4);
    void expand_vertically(int s=4);
    void updateYCoord(qreal factor);
    void updateXCoord(qreal factor);
private slots:
    void blastLoadingProgress(void *bdata);
    void blastIsLoaded(void *bdata);
};

#endif // GRAPHWIDGET_H
