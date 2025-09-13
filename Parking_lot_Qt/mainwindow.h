#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>              //for showing error/info dialogs
#include <QClipboard>               //for copy-to-clipboard function
#include "uart.h"                   //for custom serial port

//New headers added to the proj
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QVector>
#include "qextserialport.h"
#include "qextserialenumerator.h"

#include <QDateTime>

#define SERIAL_PACKET_TYPE_CONFIGURE_TEST   0
#define SERIAL_PACKET_TYPE_POWER_TEST   0

class GraphWidget;
class Node;
class Edge;


//Forward declaration of auto-generated UI class by Qt
namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
    friend class Node;

public:
    //Constructor and desctructor declaration
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    //Override Qt event handler to catch language change or theme update....
    void changeEvent(QEvent *e);

private:
    Ui::MainWindow *ui;
    QextSerialPort port;
    QMessageBox error;              //Dialog for displaying error messages
    QMessageBox pop_up;
    GraphWidget *widget;
    bool m_record;                  //Flag for whether recording is active
    Uart *uart;                     //Pointer for uart object

    std::vector<Node *> nodes;
    std::vector<Edge *> edges;
    std::vector<Edge *> last_path;

    struct SensorState {
        double light = -1;
        double distance = -1;

        bool currentOccupied = true;       // current state
        bool lastStableOccupied = true;    // last stable state
        QDateTime lastStatusChangeTime;     // last status change time
    };

    QMap<int, SensorState> nodeStates;

    void evaluateParkingStatus(int nodeID);
    void createDockWindows();
    void updateGraphBoxStyle();
    void resetSystem();

    // Number of nodes in the graph
    static const int NODE_COUNT = 8;

    // Initial positions for each node, fixed as QPointF
    const QPointF nodePositions[NODE_COUNT] = {
        QPointF(0, -200),        // Node 0  Master node position
        QPointF(-175, -75),      // Node 1  Head position 1
        QPointF(0, -50),         // Node 2  Head position 2
        QPointF(175, -50),       // Node 3  Head position 3
        QPointF(-250, 100),      // Node 4  
        QPointF(-150, 75),       // Node 5
        QPointF(0, 150),         // Node 6
        QPointF(175, 125)        // Node 7
    };



private slots:
    void on_pushButton_close_clicked();
    void on_pushButton_open_clicked();
    void on_pushButton_reset_clicked();
    void receive();
    void send(QByteArray data);
    void on_pushButtonSetPower_clicked();
};


class GraphWidget : public QGraphicsView
{
    Q_OBJECT

public:
    //Constructor initializes the widget and links it to the parent(MainWindow)
    GraphWidget(QWidget *parent = nullptr);

    void startLayoutRefresh();

protected:
    //Update the position of nodes and refresh the visual layout
    void timerEvent(QTimerEvent *event) override;

    //Custom background rendering for graph area
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    //Timer ID used to track the layout update timer
    int timerId = 0;

    //Pinter to central node
    Node *centerNode;
};

class Node : public QGraphicsItem
{

public:
    enum NodeType {
        Master,
        ClusterHead,
        Normal,
        Offline   // Add node status offline and paint the node red
    };
    //This constructor links the node to the graph view and its parent Mainwindow
    Node(GraphWidget *graphWidget, MainWindow *w, NodeType ntype = Normal);

    //Add an edge to the node
    void addEdge(Edge *edge);

    //Return all edges connected to the node
    QVector<Edge *> &edges();

    //Custom type identifier used by Qt's graphics view system
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    //Determine where the node should move next
    void calculateForces();

    //Apply  new position calculated in calculateForces
    bool advancePosition();

    //Redraws the rectangular bound
    QRectF boundingRect() const override;

    //Defines the precise shape of the node
    QPainterPath shape() const override;

    //Accessor for the node type
    void setType(NodeType newType);

    NodeType getType() const;

    //Responsible for drawing the node itself
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;


protected:
    //Used to notify the graph widget when its node is moved
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    //List of all edges connected to the node
    QVector<Edge *> edgeList;

    //Next position for the node to move to
    QPointF newPos;

    //Reference to the parent graph view
    GraphWidget *graph;

    //Reference to the Mainwindow
    MainWindow *parentWindow;

    NodeType ntype;
};

class Edge : public QGraphicsItem
{
public:

    //Constructor of the class, contains sourcenode, dstnode and edgetype(new link, lost link or packet path)
    Edge(Node *sourceNode, Node *destNode, unsigned int edgeType);

    //Accessor for source and destination node
    Node *sourceNode() const;
    Node *destNode() const;

    //Recalculate the edge if nodes were moved
    void adjust();

    //Custom type ID
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

protected:

    //Defines the rectangular area that contains the edges
    QRectF boundingRect() const override;

    //Renders the edge lines
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    //Pointer to source and destination nodes of the edge
    Node *source, *dest;

    //edge_type: 0 = New Link; 1 = Lost Link; 2 = Packet Path
    unsigned int edge_type;

    QPointF sourcePoint;
    QPointF destPoint;

    //Arrowhead size for directional drawing
    qreal arrowSize = 10;
};

#endif // MAINWINDOW_H
