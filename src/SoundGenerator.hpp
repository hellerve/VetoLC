#ifndef SOUNDGENERATOR
#define SOUNDGENERATOR

#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QAudioFormat>
//#include <QtQml/QQmlEngine>
#include <QFile>
#include <QAction>

/**
 * @brief The SoundGenerator class
 * @author Veit Heller(s0539501) & Tobias Brosge(s0539713)
 *
 * A subclass of QObject that implements an environment for sound
 * live coding while keeping the main codeeditor thread clean by
 * dispatching. QML-based.
 */
class SoundGenerator : public QObject{
Q_OBJECT

public:
    SoundGenerator(const QString &, const QString &);
    void run();
    bool updateCode(const QString &, const QString &);

private:
    int addToOutputs(QFile, QAudioFormat);
    void removeFromOutputs(int);
    void changeOutputAt(int, QFile*);
    void loop();
    void rewind();
    void forward();
    void updateInfo();
    void updateTime();
    void playPause();

    QList<QAudioOutput*> outputs;
    QString name;
    QString instructions;
    bool stopflag;
    QString ownExcept;
    QAction* abortAction;
//    QQmlEngine* engine;

private Q_SLOTS:
    void terminated();

Q_SIGNALS:
    void doneSignal(QString);
};

#endif // SOUNDGENERATOR
