#ifndef BASETAXNODE_H
#define BASETAXNODE_H

#include "threadsafelist.h"

#include <QList>
#include <QString>

class GraphView;
class GraphNode;

class BaseTaxNode;

class ChildrenList : public ThreadSafeList<BaseTaxNode *>
{

};

class BaseTaxNode
{
protected:
    bool collapsed;
    GraphNode *gnode;
public:
    BaseTaxNode(bool collapsed);
    virtual ~BaseTaxNode(){}
    ChildrenList children;
    BaseTaxNode *parent;
    virtual BaseTaxNode *addChild(BaseTaxNode *node);
    virtual bool isCollapsed() {return collapsed; }
    void setCollapsed(bool b, bool updateGnode);
    virtual void mergeWith(BaseTaxNode *other, GraphView *gview);

    virtual void removeGnode() { gnode = NULL; }
    virtual GraphNode *getGnode() { return gnode; }
    virtual GraphNode * createGnode(GraphView *gv) = 0;
    // Abstract methods
    virtual QString getName() = 0;
    virtual qint32 getId() = 0;
    virtual int getLevel() = 0;
    virtual void setLevel(int level) = 0;
    virtual QString getText() = 0;

    friend class GraphNode;
};

#endif // BASETAXNODE_H
