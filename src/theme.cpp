#include "theme.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
Theme::Theme(QObject *parent):QObject(parent),m_dir(QDir::homePath()+"/.local/state/omarchy/current") {
    m_timer.setSingleShot(true);m_timer.setInterval(100);
    connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this]{m_timer.start();});
    connect(&m_watcher,&QFileSystemWatcher::fileChanged,this,[this]{m_timer.start();});
    connect(&m_timer,&QTimer::timeout,this,&Theme::reload); reload();
}
void Theme::reload(){
    auto paths=m_watcher.files()+m_watcher.directories();if(!paths.isEmpty())m_watcher.removePaths(paths);
    QString ancestor=m_dir;while(!QFileInfo::exists(ancestor)&&ancestor!="/")ancestor=QFileInfo(ancestor).absolutePath();
    for(const auto &p:{ancestor,QFileInfo(ancestor).absolutePath(),m_dir,m_dir+"/theme",m_dir+"/theme/colors.toml"})if(QFileInfo::exists(p))m_watcher.addPath(p);
    QFile f(m_dir+"/theme/colors.toml");if(f.open(QIODevice::ReadOnly)){auto m=QRegularExpression(R"(^\s*accent\s*=\s*["'](#[0-9a-fA-F]{6})["'])",QRegularExpression::MultilineOption).match(QString::fromUtf8(f.readAll()));if(m.hasMatch()&&m.captured(1)!=m_accent){m_accent=m.captured(1);emit changed();}}
    if(qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))return;
    QProcess p;p.start(QStandardPaths::findExecutable("hyprctl"),{"-j","getoption","decoration:rounding"});if(p.waitForFinished(250)){auto json=QJsonDocument::fromJson(p.readAllStandardOutput()).object();int r=json.value("int").toInt();if(r!=m_radius){m_radius=r;emit changed();}}
}
