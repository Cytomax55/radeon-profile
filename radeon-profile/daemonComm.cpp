
// copyright marazmista @ 12.05.2014

#include "daemonComm.h"
#include <QDebug>

const QString confirmationString("7#1#");

DaemonComm::DaemonComm() : signalSender(new QLocalSocket(this)) {
    feedback.setDevice(signalSender);
    feedback.setVersion(QDataStream::Qt_5_7);
    connect(signalSender,SIGNAL(readyRead()), this, SLOT(receiveFromDaemon()));
}

DaemonComm::~DaemonComm() {
    delete signalSender;
}

void DaemonComm::connectToDaemon() {
    qDebug() << "Connecting to daemon...";
    signalSender->abort();
    signalSender->connectToServer("/tmp/radeon-profile-daemon-server");
}

void DaemonComm::disconnectDaemon() {
    if (signalSender != nullptr)
        signalSender->close();
}

void DaemonComm::sendCommand(const QString command) {
    if(signalSender->write(command.toLatin1(),command.length()) == -1) // If sending signal fails
        qWarning() << "Failed sending signal: " << command;
}

void DaemonComm::receiveFromDaemon() {
    feedback.startTransaction();

    QString confirmMsg ;
    feedback >> confirmMsg;

    if (!feedback.commitTransaction())
        return;

    if (confirmMsg.toLatin1() == confirmationString)
        sendCommand(confirmMsg.toLatin1());

    qDebug() << confirmMsg.toLatin1();
}
