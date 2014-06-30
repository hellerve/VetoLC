#include "Backend.hpp"

// TODO:
// - Think about all

using namespace Instances;

/**
 * @brief Backend::Backend
 * @param parent
 *
 * The constructor of the Backend class.
 * Initializes the editor window list and the thread list as well
 * as the settings backend.
 */
Backend::Backend(QObject *parent) : QObject(parent){ }

/**
 * @brief Backend::~Backend
 *
 * The destructor of the Backend class.
 * Eliminates all the threads that were orphaned
 * when all the windows closed.
 */
Backend::~Backend(){
    foreach(LiveThread *thread, threads.values())
        if(thread){
            if(thread->isRunning())
                thread->terminate();
            delete thread;
        }
    if(!ids.isEmpty())
        qDebug() << "Orphaned children: " << ids;
}

/**
 * @brief Backend::addChild
 * @param child
 *
 * Is called by one of the editor window instances;
 * enlists the child and creates an empty thread entry
 * in the list so that the two correlate. Emits the
 * added() signal so that the child can adjust its UI
 * to the current settings.
 */
void Backend::addInstance(IInstance *instance, bool removeSettings){
    int id = instance->ID;
    if(instances.contains(id))
        return;
    if(removeSettings)
        settings.removeSettings(id);
    qDebug() << "Child (" << id << " : " << instance << ") added to backend (" << this << ")";
    instances.insert(id, instance);
    connect(instance, SIGNAL(closing(IInstance*)),  this, SLOT(instanceClosing(IInstance *)));
    connect(instance, SIGNAL(destroyed(QObject*)),  this, SLOT(instanceDestroyed(QObject *)));
    connect(instance, SIGNAL(runCode(IInstance*)),  this, SLOT(instanceRunCode(IInstance *)));
    connect(instance, SIGNAL(stopCode(IInstance*)), this, SLOT(instanceStopCode(IInstance *)));
    connect(instance, SIGNAL(changeSetting (IInstance*, const QString, const QVariant&)),
            this, SLOT(instanceChangedSetting(IInstance*, const QString&, const QVariant&)));
    connect(instance, SIGNAL(getSetting    (IInstance*, const QString, QVariant&)),
            this, SLOT(instanceRequestSetting(IInstance*, const QString&, QVariant&)));
    connect(instance, SIGNAL(changeSettings(IInstance*, const QHash<QString,QVariant>&)),
            this, SLOT(instanceChangedSettings(IInstance*, const QHash<QString,QVariant>&)));
    connect(instance, SIGNAL(getSettings   (IInstance*, QHash<QString,QVariant>&)),
            this, SLOT(instanceRequestSettings(IInstance*, QHash<QString,QVariant>&)));
    connect(instance, SIGNAL(closeAll()), this, SLOT(childSaidCloseAll()));
    connect(instance, SIGNAL(openSettings(IInstance*)), this, SLOT(settingsWindowRequested(IInstance*)));
    connect(instance, SIGNAL(openHelp(IInstance*)), this, SLOT(openHelp(IInstance*)));
    ids.append(id);
    saveIDs();
}

/**
 * @brief Backend::nextID
 * @return Free to use id
 *
 * Lookup the first free ID for a new Instance
 */
int Backend::nextID(){
    int id = 0;
    while(ids.contains(id))
        ++id;
    return id;
}

/**
 * @brief Backend::loadIds
 * @return A list of used IDs
 *
 * Lookup the saved id-list, for that settings
 * should exists
 */
QList<int> Backend::loadIds()
{
    QVariantList ids = settings.getSettingsFor("Instances",
                                               QVariantList()).toList();
    QList<int> res;
    for(QVariant id : ids){
        bool ok;
        int i = id.toInt(&ok);
        if(ok)
            res.append(i);
    }
    return res;
}

void Backend::instanceClosing(IInstance *instance)
{
    removeInstance(instance);
}

void Backend::instanceDestroyed(QObject *instance)
{
    int id = ((IInstance*)instance)->ID;
    instances.remove(id);
    removeInstance(id, false);
}

/**
 * @brief Backend::removeChild
 * @param child
 *
 * Is called by one of the editor window instances;
 * removes the child from the list and closes the thread.
 * Removes all the settings that belong to the current child.
 * BUG: When the settings of the next children are updated,
 *      the settings window will display the settings of the
 *      killed child. This will result in confusion of the user.
 * TODO: Fix bug!
 */
bool Backend::removeInstance(Instances::IInstance *instance, bool removeSettings){
    return removeInstance(instance->ID, removeSettings);
}

bool Backend::removeInstance(int id, bool removeSettings){
    if(instances.contains(id)){
        disconnect(instances[id], SIGNAL(destroyed(QObject*)), this, SLOT(instanceDestroyed(QObject*)));
        if(!instances[id]->close()){
            connect(instances[id], SIGNAL(destroyed(QObject*)), this, SLOT(instanceDestroyed(QObject*)));
            return false;
        }
        instances[id]->deleteLater();
        instances.remove(id);
    }
    terminateThread(id);
    if(removeSettings && ids.size() > 1){
        settings.removeSettings(id);
        ids.removeOne(id);
        saveIDs();
    }
    return true;
}

/**
 * @brief Backend::childSaidCloseAll
 *
 * Is called by one of the editor window instances;
 * when a user requests to exit the application, this
 * will tell all the children to terminate.
 */
void Backend::childSaidCloseAll(){
    QList<int> notRemoved = ids;
    for(int id : ids){
        disconnect(instances[id], SIGNAL(destroyed(QObject*)), this, SLOT(instanceDestroyed(QObject*)));
        if(removeInstance(id, false))
            notRemoved.removeOne(id);
    }
    if(!notRemoved.empty()){
        for(int id : ids)
            if(!notRemoved.contains(id))
                settings.removeSettings(id);
        ids = notRemoved;
    }
    saveIDs();
}

/**
 * @brief Backend::childExited
 * @param child
 * @param file
 *
 * Is called by one of the editor window instances;
 * when the child reacts to the closedAction, it is removed
 * from the list.
 */
void Backend::childExited(IInstance *child, QString file){
    Q_UNUSED(child);
    Q_UNUSED(file);
//    saveSettings(child, file);
//    children.removeOne(child);
}


/**
 * @brief Backend::getNumOfChildren
 * @return number of children
 *
 * Gets the window count from the settings.
 */
int Backend::getNumOfChildren(){
    int set = settings.getSettingsFor("WindowCount", 1).toInt();
    return set < 1 ? 1 : set;
}

/**
 * @brief Backend::getSettings
 * @param child
 * @return a list of current settings
 *
 * Gets all settings for a specific window.
 */
QHash<QString, QVariant> Backend::getSettings(IInstance* instance)
{
    return getSettings(instance->ID);
}

QHash<QString, QVariant> Backend::getSettings(int id)
{
    return settings.getSettings(id);
}

/**
 * @brief Backend::settingsWindowRequested
 * @param child
 *
 * Creates a settings window instance.
 */
void Backend::settingsWindowRequested(IInstance *instance){
    SettingsWindow settingsWin(instance->ID);
    settingsWin.exec();
}

/**
 * @brief Backend::openHelp
 *
 * Opens a help window in HTML.
 */
void Backend::openHelp(IInstance *){
    QDesktopServices::openUrl(
                QUrl(QCoreApplication::applicationDirPath().append(
                         "/../LiveCodingEditor/html/help.html"),
                QUrl::TolerantMode));
}


/**
 * @brief Backend::removeSettings
 * @param child
 *
 * removes the settings for a specific file.
 */
void Backend::removeSettings(IInstance* instance){
    settings.removeSettings(instance->ID);
}

/**
 * @brief Backend::isLast
 * @return true if the child is the last one, false otherwise
 *
 * checks whether there is only one or no child in the list.
 */
bool Backend::isLast(){
    return ids.length() < 2;
}

/**
 * @brief Backend::instanceRunCode
 *
 * reacts to the run SIGNAL by running the code(duh) that is
 * in the editor at the moment.
 */

void Backend::instanceRunCode(IInstance *instance)
{
    int id = instance->ID;
    //qDebug() << id;
    if(threads.contains(id)){
        bool worked = threads[id]->updateCode(instance->title(), instance->sourceCode());
        if(!worked)
            instances[id]->reportError("Code is faulty.");
    }else{
        bool ok;
        int compiler = settings.getSettingsFor("UseCompiler", -1, id).toInt(&ok);
        if(!ok)
            compiler = -1;
        switch(compiler){
            case 0:
                this->runPySoundFile(instance);
                break;
            case 1:
                this->runQtSoundFile(instance);
                break;
            case 2:
                this->runGlFile(instance);
                break;
            case 3:
                this->runPyFile(instance);
                break;
            default:
                instance->codeStoped();
                instance->reportError("Compiler not found.");
        }
    }
}

void Backend::instanceStopCode(IInstance *instance)
{
    terminateThread(instance->ID);
}

void Backend::instanceChangedSetting(IInstance *instance, const QString &key, const QVariant &value)
{
    settings.saveSettingsFor(instance->ID, key, value);
}

void Backend::instanceRequestSetting(IInstance *instance, const QString &key, QVariant &value)
{
    value = settings.getSettingsFor(key, value, instance->ID);
}

void Backend::instanceChangedSettings(IInstance *instance, const QHash<QString, QVariant> &set)
{
    settings.saveSettingsFor(instance->ID, set);
}

void Backend::instanceRequestSettings(IInstance *instance, QHash<QString, QVariant> &set)
{
    set = settings.getSettings(instance->ID);
    //qDebug() << "Request Settings for" << instance->ID << ":" << set;
}

/**
 * @brief Backend::runPyFile
 * @param filename
 * @param instructions
 * @param index
 *
 * Creates a thread that executes Python scripts.
 */
void Backend::runPyFile(IInstance *instance){
    PyLiveThread *thread = new PyLiveThread(instance->ID, this);
    connect(thread, SIGNAL(doneSignal(PyLiveThread*, QString)),
            this, SLOT(getExecutionResults(PyLiveThread*, QString)));
    thread->initialize(instance->title(), instance->sourceCode());
    thread->start();
    threads.insert(thread->ID, thread);

}

/**
 * @brief Backend::runQtSoundFile
 * @param filename
 * @param instructions
 * @param index
 *
 * Creates a thread that executes QT sound scripts.
 */
void Backend::runQtSoundFile(IInstance *instance){
    QtSoundThread* thread = new QtSoundThread(instance->ID, this);
    connect(thread, SIGNAL(doneSignal(QtSoundThread*, QString)),
            this, SLOT(getExecutionResults(QtSoundThread*, QString)));
    thread->initialize(instance->title(), instance->sourceCode());
    thread->start();
    threads.insert(thread->ID, thread);
}

/**
 * @brief Backend::runPySoundFile
 * @param filename
 * @param instructions
 * @param index
 *
 * Creates a thread that executes AudioPython scripts.
 */
void Backend::runPySoundFile(IInstance *instance){
    PySoundThread *thread = new PySoundThread(instance->ID, this);
    connect(thread, SIGNAL(doneSignal(PySoundThread*, QString)),
            this, SLOT(getExecutionResults(PySoundThread*, QString)));
    thread->initialize(instance->title(), instance->sourceCode());
    thread->start();
    threads.insert(thread->ID, thread);

}

/**
 * @brief Backend::runGlFile
 * @param filename
 * @param instructions
 * @param index
 *
 * Creates a thread that executes GL source code.
 */
void Backend::runGlFile(IInstance *instance){
    GlLiveThread* thread = new GlLiveThread(instance->ID, this);
    connect(thread, SIGNAL(doneSignal(GlLiveThread*, QString)),
            this, SLOT(getExecutionResults(GlLiveThread*, QString)));
    thread->initialize(instance->title(), instance->sourceCode());
    thread->start();
    threads.insert(thread->ID, thread);
}

/**
 * @brief Backend::getExecutionResults
 *
 * reacts to the done SIGNAL by terminating the thread and
 * emitting a showResults SIGNAL for the QWidgets to display
 */
void Backend::getExecutionResults(QtSoundThread* thread, QString returnedException){
    disconnect(thread, SIGNAL(doneSignal(QtSoundThread*, QString)),
            this, SLOT(getExecutionResults(QtSoundThread*, QString)));
    terminateThread(thread->ID);
    instances[thread->ID]->reportWarning(returnedException);
}
void Backend::getExecutionResults(PySoundThread* thread, QString returnedException){
    disconnect(thread, SIGNAL(doneSignal(PySoundThread*, QString)),
            this, SLOT(getExecutionResults(PySoundThread*, QString)));
    terminateThread(thread->ID);
    instances[thread->ID]->reportWarning(returnedException);
}
void Backend::getExecutionResults(PyLiveThread* thread, QString returnedException){
    disconnect(thread, SIGNAL(doneSignal(PyLiveThread*, QString)),
            this, SLOT(getExecutionResults(PyLiveThread*, QString)));
    terminateThread(thread->ID);
    instances[thread->ID]->reportWarning(returnedException);
}
void Backend::getExecutionResults(GlLiveThread* thread, QString returnedException){
    // Already gone?
    disconnect(thread, SIGNAL(doneSignal(GlLiveThread*, QString)),
            this, SLOT(getExecutionResults(GlLiveThread*, QString)));
    terminateThread(thread->ID);
    instances[thread->ID]->reportWarning(returnedException);
}

/**
 * @brief Backend::terminateThread
 * @param thread
 *
 * terminates a specific thread and deletes it from the list.
 */
void Backend::terminateThread(long id){
    //NOTE: Can a thread change its address implicitly? Check back
    if(threads.contains(id)){
        if(threads[id]->isRunning())
            threads[id]->terminate();
        threads[id]->deleteLater();
        threads.remove(id);
    }
}

void Backend::saveIDs(){
    QVariantList vids;
    for(int i : ids)
        vids.append(i);
    settings.addSettings("Instances", vids);
}