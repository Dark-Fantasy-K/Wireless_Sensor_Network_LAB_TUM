#include "mainwindow.h"
#include "ui_mainwindow.h"

//Libraries needed for implementation
#include <qdebug.h>
#include <math.h>
#include <vector>

#include <QFile>
#include <QDir>
#include <QFileDialog>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QtMath>
#include <QtWidgets>

#include <QMessageBox>
#include <QPixmap>

//Enable time display when message received
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {

    //set up ui from .ui file
    ui->setupUi(this);

    // Collect all checkboxes into a vector
    QVector<QCheckBox *> checkboxes = {
        ui->park1, ui->park2, ui->park3, ui->park4, ui->park5, ui->park6, ui->park7,
        ui->work1, ui->work2, ui->work3, ui->work4, ui->work5, ui->work6, ui->work7
    };

    // Disable mouse click for each checkbox
    for (QCheckBox *checkbox : checkboxes) {
        checkbox->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        checkbox->setFocusPolicy(Qt::NoFocus);  // Optional: Prevent keyboard focus
    }

    // Get available COM Ports
    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();
    for (int i = 0; i < ports.size(); i++) {
        QString portName = ports.at(i).portName;
        if (portName.contains("ttyACM", Qt::CaseSensitive)){
            ui->comboBox_Interface->addItem(portName.toLocal8Bit().constData());
        }
    }

    //Warning message if no USB port is found
    if (ui->comboBox_Interface->count() == 0){
        ui->textEdit_Status->insertPlainText("No USB ports available.\nPlease have another try! \n");
    }

    //Initialize the topology graph
    widget = new GraphWidget;

    createDockWindows();

    this->showMaximized();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::changeEvent(QEvent *e) {
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

//Once this button is pressed, device connected to the PC could be opened and data could be seen
void MainWindow::on_pushButton_open_clicked() {

    port.setQueryMode(QextSerialPort::EventDriven);
    //Port name has to be compatible with linux convention
    port.setPortName("/dev/" + ui->comboBox_Interface->currentText());
    port.setBaudRate(BAUD115200);
    port.setFlowControl(FLOW_OFF);
    port.setParity(PAR_NONE);
    port.setDataBits(DATA_8);
    port.setStopBits(STOP_1);
    port.open(QIODevice::ReadWrite);
    // ui->textEdit_Status->clear();

    if (!port.isOpen())
    {
        error.setText("Open port unsuccessful, try again!");
        error.show();
        return;
    }

    //Once data is sensed, trigger mainwindow receive function and read serial data
    QObject::connect(&port, SIGNAL(readyRead()), this, SLOT(receive()));

    ui->pushButton_close->setEnabled(true);         //Enable close button
    ui->pushButton_open->setEnabled(false);         //Disable open button
    ui->comboBox_Interface->setEnabled(false);      //Disable dropdown interfaces
}

void MainWindow::on_pushButton_close_clicked() {
    // === 1. Close the currently open serial port ===
    if (port.isOpen()) {
        port.close();
    }

    // === 2. Refresh the COM port list ===
    ui->comboBox_Interface->clear();  // Clear old items

    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();
    for (int i = 0; i < ports.size(); i++) {
        QString portName = ports.at(i).portName;
        if (portName.contains("ttyACM", Qt::CaseSensitive)) {
            ui->comboBox_Interface->addItem(portName.toLocal8Bit().constData());
        }
    }

    // === 3. Show warning if no ports found ===
    if (ui->comboBox_Interface->count() == 0){
        ui->textEdit_Status->append("No USB ports available.\nPlease have another try!");
    } else {
        ui->textEdit_Status->append("COM port list refreshed.");
    }

    // === 4. Update button states ===
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_open->setEnabled(true);
    ui->comboBox_Interface->setEnabled(true);
}


//Message example: Node: 1 SensorType: 1 Value: 12 Battery: 80
void MainWindow::receive() {
    static QString str;
    char ch;
    while (port.getChar(&ch)){
        str.append(ch);

        //Detect end of line and decode from here
        if (ch == '\n'){
            str.remove("\n", Qt::CaseSensitive);
            str.remove("\r");

            // Prepend current time to the incoming message string
            QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
            QString logLine = timestamp + " | " + str;

            // Append the time-stamped message to the QTextEdit for status display
            ui->textEdit_Status->append(logLine);
            // ui->textEdit_Status->ensureCursorVisible();  // Auto-scroll to the latest line
            ui->textEdit_Status->moveCursor(QTextCursor::End);

            if(str.contains("SensorType:")){
                QStringList list = str.split(QRegExp("\\s"));
                //Display information in Qt console
                qDebug() << "Parsed serial input: " << str;

                //Deals with different type of sensors
                if(!list.isEmpty()){
                    int i = 0;
                    int sensorType = list.at(i+3).toInt();
                    int nodeID = list.at(i+1).toInt();

                    // Revive node when it was marked offline
                    if (nodes.at(nodeID)->getType() == Node::Offline) {
                        nodes.at(nodeID)->setType(Node::Normal);
                        qDebug() << "Node" << nodeID << "revived, set back to Normal";
                    }

                    // When node id is fetched, set this node as active
                    switch (nodeID) {
                        case 1: ui->work1->setChecked(true); break;
                        case 2: ui->work2->setChecked(true); break;
                        case 3: ui->work3->setChecked(true); break;
                        case 4: ui->work4->setChecked(true); break;
                        case 5: ui->work5->setChecked(true); break;
                        case 6: ui->work6->setChecked(true); break;
                        case 7: ui->work7->setChecked(true); break;
                    }

                    int battery = list.at(i+7).toInt();
                    switch (nodeID) {
                    case 1: ui->battery1->display(battery); break;
                    case 2: ui->battery2->display(battery); break;
                    case 3: ui->battery3->display(battery); break;
                    case 4: ui->battery4->display(battery); break;
                    case 5: ui->battery5->display(battery); break;
                    case 6: ui->battery6->display(battery); break;
                    case 7: ui->battery7->display(battery); break;
                    }
                    qDebug() << "Battery level: " << QString::number(battery);

                    double value = list.at(i+5).toInt();
                    qDebug() << "List size " << list.size();
                    qDebug() << "List value "<< i <<" "<< list.at(i);
                    switch(sensorType){
                        //Light sensor
                        case 1:
                            nodeStates[nodeID].light = value;
                            switch (nodeID) {
                                case 1: ui->value_light1->display(value); break;
                                case 2: ui->value_light2->display(value); break;
                                case 3: ui->value_light3->display(value); break;
                                case 4: ui->value_light4->display(value); break;
                                case 5: ui->value_light5->display(value); break;
                                case 6: ui->value_light6->display(value); break;
                                case 7: ui->value_light7->display(value); break;
                            }
                            qDebug() << "Light sensor value: " << QString::number(value);
                            break;
                        //Distance sensor
                        case 2:
                            nodeStates[nodeID].distance = value;
                            switch (nodeID) {
                                case 1: ui->value_distance1->display(value); break;
                                case 2: ui->value_distance2->display(value); break;
                                case 3: ui->value_distance3->display(value); break;
                                case 4: ui->value_distance4->display(value); break;
                                case 5: ui->value_distance5->display(value); break;
                                case 6: ui->value_distance6->display(value); break;
                                case 7: ui->value_distance7->display(value); break;
                            }
                            qDebug() << "Distance sensor value: " << QString::number(value);
                            break;
                    }
                    evaluateParkingStatus(nodeID);
                    updateGraphBoxStyle();
                }
            }

            //Change of network topology -- new link is added
            //e.g. NewLink 1 -> 2
            else if (str.contains("Newlink")){
                int new_src;
                int new_dest;
                // Get the current scene from the GraphWidget to modify the visual graph
                QGraphicsScene *scene = widget->scene();

                //Extract node IDs from the string
                QStringList list = str.split(QRegExp("\\s"));
                qDebug() << "Parsed serial input: " << str;

                if (!list.isEmpty()) {
                    qDebug() << "List size: " << list.size();
                    for (int i = 0; i < list.size(); i++) {
                        // qDebug() << "List value " << i << ": " << list.at(i);
                    }
                    new_src = list.at(1).toInt();
                    //If nodeID is 255, then it should be node 0 which is the master node, and all other node numbers should be added 1
                    new_src = (new_src == 255) ? 0 : new_src+1;
                    new_dest = list.at(3).toInt();
                    new_dest = (new_dest == 255) ? 0 : new_dest+1;
                    printf("%d\n",new_src);
                    printf("%d\n",new_dest);
                    qDebug() << "New link between nodes: " << new_src << " and " << new_dest;

                    // Revive nodes if they were offline
                    if (nodes.at(new_src)->getType() == Node::Offline) {
                        nodes.at(new_src)->setType(Node::Normal); 
                        qDebug() << "Node" << new_src << "revived, set back to Normal";
                    }
                    if (nodes.at(new_dest)->getType() == Node::Offline) {
                        nodes.at(new_dest)->setType(Node::Normal); 
                        qDebug() << "Node" << new_dest << "revived, set back to Normal";
                    }

                        // for(Edge *existing_edge: edges){
                        //     if((existing_edge->sourceNode() == nodes.at(new_src))
                        //         && (existing_edge->destNode() == nodes.at(new_dest))){
                        //         if (existing_edge->scene() == scene) {
                        //             scene->removeItem(existing_edge);
                        //         }
                        //         delete existing_edge;
                        //     }
                        // }
                    
                    // === Check if edge already exists ===
                    bool exists = false;
                    for (Edge *edge : edges) {
                        if ((edge->sourceNode() == nodes.at(new_src) &&
                             edge->destNode() == nodes.at(new_dest)) ) {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists) {
                    // Add a new green edge to indicate the lost connection
                    Edge *edge = new Edge(nodes.at(new_src), nodes.at(new_dest), 0);
                    scene->addItem(edge);
                    edges.push_back(edge);
                    qDebug() << "New edge added.";
                    } 
                }
            }

            // Handle node loss notification, delete all edges connected to the node
            //e.g. LinkLost: 1
            else if (str.contains("LinkLost:")) {
                int lost;
                // Get the current scene from the GraphWidget to modify the visual graph
                QGraphicsScene *scene = widget->scene();

                //Extract node IDs from the string
                QStringList list = str.split(QRegExp("\\s"));
                qDebug() << "Parsed serial input: " << str;
                if (!list.isEmpty()) {
                    qDebug() << "List size: " << list.size();
                    for (int i = 0; i < list.size(); i++) {
                        qDebug() << "List value " << i << ": " << list.at(i);
                    }

                    lost = list.at(1).toInt() +1;
                    // Uncheck the corresponding checkbox for the lost source node
                    switch (lost) {
                        case 1: ui->work1->setChecked(false); break;
                        case 2: ui->work2->setChecked(false); break;
                        case 3: ui->work3->setChecked(false); break;
                        case 4: ui->work4->setChecked(false); break;
                        case 5: ui->work5->setChecked(false); break;
                        case 6: ui->work6->setChecked(false); break;
                        case 7: ui->work7->setChecked(false); break;
                        default:
                            qDebug() << "Warning: nodeID out of reach:" << lost;
                            break;
                    }

                    nodes.at(lost)->setType(Node::Offline);
                    qDebug() << "Node" << lost << "is offline, removing all related edges";

                    // Remove any matching existing edge from and to the lost node
                    std::vector<Edge*> remaining_edges;
                    for (Edge *edge : edges) {
                        if ((edge->sourceNode() == nodes.at(lost)) || (edge->destNode() == nodes.at(lost))){
                            if (edge->sourceNode()) {
                                auto &srcEdges = edge->sourceNode()->edges();
                                srcEdges.removeAll(edge);
                            }

                            if (edge->destNode()) {
                                auto &dstEdges = edge->destNode()->edges();
                                dstEdges.removeAll(edge);
                            }
                            scene->removeItem(edge);
                            delete edge;
                        } else {
                            remaining_edges.push_back(edge);
                        }
                    }
                    edges = remaining_edges;
                
                    // for (Edge *existing_edge : edges) {
                    //     if ((existing_edge->sourceNode() == nodes.at(lost)) ||
                    //         (existing_edge->destNode() == nodes.at(lost))) {
                    //         scene->removeItem(existing_edge);
                    //     }
                    // }
                    
                    // Show error message for 3s
                    // QLabel *errorLabel = new QLabel(this);
                    // errorLabel->setText(QString("Node %1 is offline! Save it!").arg(lost));
                    // errorLabel->setStyleSheet("QLabel {background-color: yellow; font-weight: bold;}");
                    // errorLabel->setGeometry(1300, 150, 250, 50);
                    // errorLabel->show();
                    // // Hold the error message for 5s
                    // QTimer::singleShot(5000, errorLabel, &QLabel::deleteLater);

                    evaluateParkingStatus(lost);
                    updateGraphBoxStyle();
                }
            }

            //e.g. ClusterHead: 2 3 6
            else if (str.contains("ClusterHead:")) {
                int head1;
                int head2;
                int head3;

                // Split the message
                QStringList list = str.split(QRegExp("\\s"));
                qDebug() << "Parsed serial input: " << str;

                for (int i = 0; i < list.size(); i++) {
                    qDebug() << "List value " << i << ": " << list.at(i);
                }

                head1 = list.at(1).toInt() +1;
                head2 = list.at(2).toInt() +1;
                head3 = list.at(3).toInt() +1;

                nodes.at(head1)->setPos(nodePositions[1]);  
                nodes.at(head2)->setPos(nodePositions[2]);    
                nodes.at(head3)->setPos(nodePositions[3]);

                QVector<int> heads = {head1, head2, head3};
                for (int head : heads) {
                    if (nodes.at(head)->getType() == Node::Normal) {
                        nodes.at(head)->setType(Node::ClusterHead);
                    }
                }


                qDebug() << "Assigned cluster heads: Node" << head1 << ", Node" << head2 << "and Node" << head3;

                // Start assigning positions from nodePositions[4]
                int positionIndex = 4;

                // Loop through all other nodes and assign them positions
                for (int i = 1; i <= 7; ++i) {
                    // Skip the nodes that are cluster heads
                    if (i != head1 && i != head2 && i != head3) {
                        if (nodes.at(i)->getType() == Node::ClusterHead) {
                            nodes.at(i)->setType(Node::Normal);
                        }
                        nodes.at(i)->setPos(nodePositions[positionIndex]);
                        positionIndex++;
                    }
                }
            }

            else if (str.contains("REORGANIZATION")) {
                qDebug() << "Reorganization signal received. Removing all edges.";

                QGraphicsScene *scene = widget->scene();

                // disconnect all edges from their source and destination nodes
                // and remove them from the scene
                for (Edge *edge : edges) {
                    Node *src = edge->sourceNode();
                    Node *dst = edge->destNode();

                    if (src) {
                        auto &srcEdges = src->edges();
                        srcEdges.removeAll(edge);
                    }

                    if (dst) {
                        auto &dstEdges = dst->edges();
                        dstEdges.removeAll(edge);
                    }

                    scene->removeItem(edge);
                    delete edge;
                }

                edges.clear();

                ui->textEdit_Status->append("Topology cleared due to REORGANIZATION.");
            }

            this->repaint();    // Force the GUI to refresh and reflect the latest topology changes
            str.clear();        // Reset the input buffer for the next line of serial data
        }
    }
}

//Setup the dock widget and place each node in its initial position according to their roles (master, cluster heads, normal nodes)
void MainWindow::createDockWindows()
{
    // Create a dock area labeled "Network" and allow docking on left/right sides
    QDockWidget *dock = new QDockWidget(tr("Network Topology"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // Access the graphics scene for adding nodes
    QGraphicsScene *scene = widget->scene();

    // initialize master node
    nodes.push_back(new Node(widget, this, Node::Master));

    // initialize cluster heads
    for (int i = 1; i < 4; i++) {
        nodes.push_back(new Node(widget, this, Node::ClusterHead));
    }

    // initialize other nodes as Normal
    for (int i = 4; i < 8; i++) {
        nodes.push_back(new Node(widget, this, Node::Normal));
    }

    // Add all nodes into the graphics scene for visualization
    for (int i = 0; i < 8; i++) {
        scene->addItem(nodes.at(i));
    }

    // Position the master node at the top center
    nodes.at(0)->setPos(nodePositions[0]);

    // Arrange cluster heads in the middle layer, spaced horizontally
    nodes.at(1)->setPos(nodePositions[1]);
    nodes.at(2)->setPos(nodePositions[2]);
    nodes.at(3)->setPos(nodePositions[3]);

    // Place remaining nodes under their respective cluster heads
    // Group 1: Nodes under cluster head 1
    nodes.at(4)->setPos(nodePositions[4]);
    nodes.at(5)->setPos(nodePositions[5]);

    // Group 2: Nodes under cluster head 2
    nodes.at(6)->setPos(nodePositions[6]);

    // Group 3: Node under cluster head 3
    nodes.at(7)->setPos(nodePositions[7]);

    // Put the GraphWidget into the dock and add it to the main window
    dock->setWidget(widget);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}


void MainWindow::send(QByteArray data) {
    port.write(data);

    qint64 bytesWritten = port.write(data);
    qDebug() << "Sent" << bytesWritten << "bytes:" << data;
}

// Function to evaluate parking lot status with time-based smoothing
void MainWindow::evaluateParkingStatus(int nodeID)
{
    auto &state = nodeStates[nodeID];  // Access the sensor readings and status tracking

    // Determine the new parking status based on the current sensor values
    bool newOccupied = (state.light < 100 && state.distance < 50);

    // Only process if both sensors have valid data
    if (state.light > 0 && state.distance > 0) {

        // If the new reading differs from the last detected state, update the timestamp
        if (newOccupied != state.currentOccupied) {
            state.currentOccupied = newOccupied;
            state.lastStatusChangeTime = QDateTime::currentDateTime();
        }

        // If the stable state hasn't been updated yet, check how long the new status has persisted
        if (state.lastStableOccupied != state.currentOccupied) {
            qint64 elapsed = state.lastStatusChangeTime.msecsTo(QDateTime::currentDateTime());

            // Confirm the state change only if it has remained stable for more than 5 seconds
            if (elapsed > 5000) {
                state.lastStableOccupied = state.currentOccupied;

                // Update the checkbox state: checked = free, unchecked = occupied
                switch (nodeID) {
                case 1: ui->park1->setChecked(!state.lastStableOccupied); break;
                case 2: ui->park2->setChecked(!state.lastStableOccupied); break;
                case 3: ui->park3->setChecked(!state.lastStableOccupied); break;
                case 4: ui->park4->setChecked(!state.lastStableOccupied); break;
                case 5: ui->park5->setChecked(!state.lastStableOccupied); break;
                case 6: ui->park6->setChecked(!state.lastStableOccupied); break;
                case 7: ui->park7->setChecked(!state.lastStableOccupied); break;
                }

                // Log the new confirmed parking state
                if (state.lastStableOccupied) {
                    ui->textEdit_Status->append(QString("Node %1: Car detected - spot occupied").arg(nodeID));
                } else {
                    ui->textEdit_Status->append(QString("Node %1: No car - spot available").arg(nodeID));
                }

                // Optionally refresh the graph box background
                updateGraphBoxStyle();
            }
        }
    }
}

// Set graphbox to green if parking is available and red if slot is not available or node is not working
// Update the background color of each slot depending on the checkbox states
void MainWindow::updateGraphBoxStyle() {
    // Iterate over slot_1 to slot_8
    for (int i = 1; i <= 7; i++) {
        // Dynamically access park and work checkboxes using QObject::findChild
        QCheckBox *park = findChild<QCheckBox *>(QString("park%1").arg(i));
        QCheckBox *work = findChild<QCheckBox *>(QString("work%1").arg(i));
        QGroupBox *slot = findChild<QGroupBox *>(QString("slot_%1").arg(i));

        // Safety check in case any object is not found
        if (!park || !work || !slot)
            continue;

        // Determine the background color based on checkbox states
        if (park->isChecked() && work->isChecked()) {
            // If the parking sensor detects a car, set to green
            slot->setStyleSheet("QGroupBox { background-color: lightgreen; }");
        } else if (!park->isChecked() && work->isChecked()) {
            // If the work sensor is NOT connected, set to red
            slot->setStyleSheet("QGroupBox { background-color: lightcoral; }");
        } else {
            // Default case: reset to no background
            slot->setStyleSheet("");
        }
    }
}


void MainWindow::on_pushButtonSetPower_clicked()
{
    QByteArray data = QByteArray((int) 6, (char) 0);
    data.insert(0, "cmd:");
    data[4] = SERIAL_PACKET_TYPE_CONFIGURE_TEST;
    data[5] = (signed char) ui->spinBoxPower->value();
    this->send(data);
}

void MainWindow::on_pushButton_reset_clicked()
{
    resetSystem();
}

void MainWindow::resetSystem()
{
    // === 0. Close current COM port ===
    if (port.isOpen()) {
        port.close();
    }

    // === 1. Reset checkboxes ===
    QVector<QCheckBox *> checkboxes = {
        ui->park1, ui->park2, ui->park3, ui->park4,
        ui->park5, ui->park6, ui->park7,
        ui->work1, ui->work2, ui->work3, ui->work4,
        ui->work5, ui->work6, ui->work7
    };
    for (QCheckBox *checkbox : checkboxes) {
        checkbox->setChecked(false);
    }

    // === 2. Reset LCD displays ===
    QVector<QLCDNumber *> lcds = {
        ui->value_light1, ui->value_light2, ui->value_light3, ui->value_light4,
        ui->value_light5, ui->value_light6, ui->value_light7,
        ui->value_distance1, ui->value_distance2, ui->value_distance3, ui->value_distance4,
        ui->value_distance5, ui->value_distance6, ui->value_distance7,
        ui->battery1, ui->battery2, ui->battery3, ui->battery4,
        ui->battery5, ui->battery6, ui->battery7
    };
    for (QLCDNumber *lcd : lcds) {
        lcd->display(0);
    }

    // === 3. Clear status output ===
    ui->textEdit_Status->clear();
    ui->textEdit_Status->append("GUI reset done.");

    // === 4. Clear and reset comboBox_Interface ===
    ui->comboBox_Interface->clear();
    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();
    for (int i = 0; i < ports.size(); i++) {
        QString portName = ports.at(i).portName;
        if (portName.contains("ttyACM", Qt::CaseSensitive)) {
            ui->comboBox_Interface->addItem(portName.toLocal8Bit().constData());
        }
    }

    // Clear background color of all graphbox
    for (int i = 1; i < 9; i++) {
        QGroupBox *slot = findChild<QGroupBox *>(QString("slot_%1").arg(i));
        if (slot) {
            slot->setStyleSheet("");
        }
    }

    // Show warning if no ports found
    if (ui->comboBox_Interface->count() == 0){
        ui->textEdit_Status->append("No USB ports available.\nPlease have another try!");
    }

    // Re-enable comboBox and buttons
    ui->comboBox_Interface->setEnabled(true);
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_close->setEnabled(false);

    // === 5. Clear existing graph ===
    if (widget) {
        delete widget;
        widget = nullptr;
    }

    QList<QDockWidget *> docks = findChildren<QDockWidget *>();
    for (QDockWidget *dock : docks) {
        removeDockWidget(dock);
        delete dock;
    }

    nodes.clear();
    edges.clear();

    // === 6. Recreate GraphWidget and network topology ===
    widget = new GraphWidget;
    createDockWindows();

    // === 7. Re-maximize if needed ===
    this->showMaximized();
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------
// Constructor for GraphWidget, initializes the view and its scene
GraphWidget::GraphWidget(QWidget *parent)
    : QGraphicsView(parent)
{
    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    scene->setSceneRect(-400, -400, 800, 800);          // Define the visible area of the scene 
    setScene(scene);
    setCacheMode(CacheBackground);                      // Cache the background for better performance
    setViewportUpdateMode(BoundingRectViewportUpdate);  // Only update areas that have changed, for better performance
    setRenderHint(QPainter::Antialiasing);              // Turn on anti-aliasing to make shapes and lines smoother
    setTransformationAnchor(AnchorUnderMouse);
    scale(qreal(0.8), qreal(0.8));
    setMinimumSize(800, 800);                           // Set a minimum window size to avoid too small a view

    // Set the title shown when this view is opened as a window
    setWindowTitle(tr("Network Topology"));
}

// Start the layout refresh timer if it's not already running
void GraphWidget::startLayoutRefresh()
{
    if (!timerId)
        timerId = startTimer(50);  // 50 ms interval for layout updates
}

void GraphWidget::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);

    QVector<Node *> nodes;
    const QList<QGraphicsItem *> items = scene()->items();
    for (QGraphicsItem *item : items) {
        if (Node *node = qgraphicsitem_cast<Node *>(item))
            nodes << node;
    }

    // Ask each node to calculate the forces acting on it (repulsion/attraction)
    for (Node *node : qAsConst(nodes))
        node->calculateForces();

    // Attempt to apply the computed positions, check if any node actually moved
    bool layoutChanged = false;
    for (Node *node : qAsConst(nodes)) {
        if (node->advancePosition())
            layoutChanged = true;
    }

    // If no node changed its position, stop the timer to save resources
    if (!layoutChanged) {
        killTimer(timerId);
        timerId = 0;
    }
}

// Custom background painting for the topology graph view
void GraphWidget::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);  // This implementation ignores the suggested redraw area

    // Define the full scene area to apply the background
    const QRectF sceneBounds = sceneRect();

    // Draw a background gradient from top-left to bottom-right
    QLinearGradient backgroundGradient(sceneBounds.topLeft(), sceneBounds.bottomRight());
    backgroundGradient.setColorAt(0, Qt::white);       // Lighter at the top-left
    backgroundGradient.setColorAt(1, Qt::lightGray);   // Darker at the bottom-right

    // Fill the visible area with the gradient
    painter->fillRect(rect.intersected(sceneBounds), backgroundGradient);

    // Draw the border of the scene area
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(sceneBounds);
}


// -------------------------------------------------------------------------------------------------------------------------------------------------------------
//Function for handling nodes

Node::Node(GraphWidget *graphWidget, MainWindow *w, NodeType ntype_)
    : graph(graphWidget), ntype(ntype_)
{
    parentWindow = w;
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setZValue(-1);
}

// Return a list of all edges connected to this node
QVector<Edge *> &Node::edges()
{
    return edgeList;
}

void Node::addEdge(Edge *edge)
{
    edgeList << edge;
    edge->adjust();
}

void Node::calculateForces()
{
    if (!scene() || scene()->mouseGrabberItem() == this) {
        newPos = pos();
        return;
    }

    QRectF sceneRect = scene()->sceneRect();
    newPos = pos();
    newPos.setX(qMin(qMax(newPos.x(), sceneRect.left() + 10), sceneRect.right() - 10));
    newPos.setY(qMin(qMax(newPos.y(), sceneRect.top() + 10), sceneRect.bottom() - 10));
}

bool Node::advancePosition()
{
    if (newPos == pos())
        return false;

    setPos(newPos);
    return true;
}

QRectF Node::boundingRect() const
{
    qreal adjust = 2;
    return QRectF( -20 - adjust, -20 - adjust, 43 + adjust, 43 + adjust);
}

QPainterPath Node::shape() const
{
    QPainterPath path;
    path.addEllipse(-20, -20, 40, 40);
    return path;
}

void Node::setType(NodeType newType) {
    ntype = newType;
    update();
}

Node::NodeType Node::getType() const {
    return ntype;
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change) {
    case ItemPositionHasChanged:
        for (Edge *edge : qAsConst(edgeList))
            edge->adjust();
        graph->startLayoutRefresh();
        break;
    default:
        break;
    };

    return QGraphicsItem::itemChange(change, value);
}

void Node::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // Remove any pen outline initially (used for base fill)
    painter->setPen(Qt::NoPen);

    // Set a dark gray background as the base layer for the node
    painter->setBrush(Qt::darkGray);
    painter->drawEllipse(-17, -17, 40, 40);

    // Create a radial gradient for color effect
    QRadialGradient gradient(-3, -3, 20);

    // Get the list of all nodes from the main window to find this node's index
    std::vector<Node*> nodes = parentWindow->nodes;
    std::vector<Node*>::iterator it = std::find(nodes.begin(), nodes.end(), this);
    int index = std::distance(nodes.begin(), it);

    // Node coloring based on role
    switch (ntype) {
    case Master:
        // Master node: deep blue
        gradient.setColorAt(0, QColor(70, 130, 180));    // Steel Blue
        gradient.setColorAt(1, QColor(30, 60, 90));      // Darker blue
        break;
    case ClusterHead:
        // Cluster heads: light blue
        gradient.setColorAt(0, QColor(135, 206, 250));  // Light Sky Blue
        gradient.setColorAt(1, QColor(70, 130, 180));   // Steel Blue
        break;
    case Normal:
        // Regular nodes: light orange
        gradient.setColorAt(0, QColor(255, 204, 153));  // Light Orange
        gradient.setColorAt(1, QColor(205, 133, 63));   // Peru
        break;
    case Offline:
        // Offline nodes: red
        gradient.setColorAt(0, QColor(255, 99, 71));  // Tomato
        gradient.setColorAt(1, QColor(139, 0, 0));   // Dark red
        break;
    }

    // Apply the gradient to the painter
    painter->setBrush(gradient);

    // Draw the main outline of the node
    painter->setPen(QPen(Qt::black, 0));
    painter->drawEllipse(-20, -20, 40, 40);

    // Draw node ID number on top of the node
    QFont font = painter->font();
    font.setPointSize(12);
    painter->setFont(font);
    painter->drawText(-5, 5, QString::number(index));
}


// -------------------------------------------------------------------------------------------------------------------------------------------------------------
// Constructor for the Edge class, initializes the edge between two nodes and sets its type
Edge::Edge(Node *sourceNode, Node *destNode, unsigned int edgeType)
    : source(sourceNode), dest(destNode)
{
    setAcceptedMouseButtons(Qt::NoButton);
    source->addEdge(this);
    dest->addEdge(this);

    // Store the edge type (e.g., new link, lost link)
    edge_type = edgeType;

    // Compute the initial positions for rendering the edge
    adjust();
}

Node *Edge::sourceNode() const
{
    return source;
}

Node *Edge::destNode() const
{
    return dest;
}

// Update the source and destination points of the edge for drawing
void Edge::adjust()
{
    // If either node is missing, skip adjustment
    if (!source || !dest)
        return;

    // Compute a straight line between the source and destination node positions
    QLineF line(mapFromItem(source, 0, 0), mapFromItem(dest, 0, 0));
    qreal length = line.length();

    // Notify the graphics system that the item's shape will change
    prepareGeometryChange();

    // Offset the edge endpoints slightly away from the node centers to avoid drawing directly over the node graphics
    qreal nodeRadius = 20;      // Change this value if node radius changes
    if (length > qreal(20.)) {
        QPointF edgeOffset((line.dx() * nodeRadius) / length, (line.dy() * nodeRadius) / length);
        sourcePoint = line.p1() + edgeOffset;
        destPoint = line.p2() - edgeOffset;
    } else {
        // If nodes are too close, collapse the edge to a single point
        sourcePoint = destPoint = line.p1();
    }
}

// Return the bounding rectangle for this edge, used by Qt to optimize redraws and event detection
QRectF Edge::boundingRect() const
{
    if (!source || !dest)
        return QRectF();

    qreal penWidth = 1;
    qreal extra = (penWidth + arrowSize) / 2.0;

    // Create a rectangle that spans from the source point to the destination point
    QRectF rawBounds(sourcePoint, QSizeF(destPoint.x() - sourcePoint.x(),
                                     destPoint.y() - sourcePoint.y()));

    return rawBounds.normalized().adjusted(-extra, -extra, extra, extra);
}

// Custom painting of the edge line and arrow head
void Edge::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // If either endpoint is missing, skip drawing
    if (!source || !dest)
        return;

    // Create a straight line from the source to the destination point
    QLineF line(sourcePoint, destPoint);

    // If the line has no length, there's nothing to draw
    if (qFuzzyCompare(line.length(), qreal(0.)))
        return;

    // Set the pen style based on the edge type (color and line style)
    switch (this->edge_type) {
    case 0: painter->setPen(QPen(Qt::green, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        break;
    case 1: painter->setPen(QPen(Qt::red, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        break;
    }

    // Draw the main line of the edge
    painter->drawLine(line);

    // Calculate the angle of the line to orient the arrowhead correctly
    double angle = std::atan2(-line.dy(), line.dx());

    // Compute the two side points of the arrowhead triangle at the destination
    QPointF destArrowP1 = destPoint + QPointF(sin(angle - M_PI / 3) * arrowSize,
                                              cos(angle - M_PI / 3) * arrowSize);
    QPointF destArrowP2 = destPoint + QPointF(sin(angle - M_PI + M_PI / 3) * arrowSize,
                                              cos(angle - M_PI + M_PI / 3) * arrowSize);

    // Set the arrowhead fill color to match the edge color
    switch (this->edge_type) {
    case 0: painter->setBrush(Qt::green);       // New link
        break;
    case 1: painter->setBrush(Qt::red);         // Lost link
        break;
    }

    // Draw the arrowhead as a filled triangle
    painter->drawPolygon(QPolygonF() << line.p2() << destArrowP1 << destArrowP2);
}


