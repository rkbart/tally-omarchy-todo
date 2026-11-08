#include "theme.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
Theme::Theme(QObject *parent):QObject(parent),m_dir(QDir::homePath()+"/.local/state/omarchy/current") {
    QSettings settings("Tally","Tally");
    m_selectedTheme=settings.value("appearance/theme","Follow system").toString();
    if(!themeOptions().contains(m_selectedTheme))m_selectedTheme="Follow system";
    m_timer.setSingleShot(true);m_timer.setInterval(100);
    connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this]{m_timer.start();});
    connect(&m_watcher,&QFileSystemWatcher::fileChanged,this,[this]{m_timer.start();});
    connect(&m_timer,&QTimer::timeout,this,&Theme::reload); reload();
}
void Theme::reload(){
    auto paths=m_watcher.files()+m_watcher.directories();if(!paths.isEmpty())m_watcher.removePaths(paths);
    QString ancestor=m_dir;while(!QFileInfo::exists(ancestor)&&ancestor!="/")ancestor=QFileInfo(ancestor).absolutePath();
    for(const auto &p:{ancestor,QFileInfo(ancestor).absolutePath(),m_dir,m_dir+"/theme",m_dir+"/theme/colors.toml"})if(QFileInfo::exists(p))m_watcher.addPath(p);
    bool didChange=false;
    auto assign=[&didChange](QString &target,const QString &value){if(target!=value){target=value;didChange=true;}};

    QHash<QString,QString> values;
    bool systemLight=false;
    QFile f(m_dir+"/theme/colors.toml");
    if(f.open(QIODevice::ReadOnly)) {
        const QString raw=QString::fromUtf8(f.readAll());
        const QRegularExpression line(R"(^\s*([A-Za-z0-9_-]+)\s*=\s*["']?(#[0-9a-fA-F]{6})["']?)",QRegularExpression::MultilineOption);
        auto it=line.globalMatch(raw);
        while(it.hasNext()){const auto m=it.next();values.insert(m.captured(1),m.captured(2));}
        systemLight=QRegularExpression(R"(^\s*mode\s*=\s*["']light["']\s*$)",QRegularExpression::MultilineOption|QRegularExpression::CaseInsensitiveOption).match(raw).hasMatch();
    }
    const auto systemColor=[&values](const QString &key,const QString &fallback){return values.value(key,fallback);};
    if(m_selectedTheme=="Tally Dark") {
        assign(m_accent,"#FFD60A");assign(m_background,"#202125");assign(m_darkBackground,"#101113");
        assign(m_darkerBackground,"#191a1d");assign(m_lighterBackground,"#2b2c30");
        assign(m_foreground,"#f1f0eb");assign(m_darkForeground,"#74767c");assign(m_lightForeground,"#a0a1a6");
        assign(m_muted,"#a0a1a6");assign(m_selection,"#2b2c30");assign(m_red,"#ff837b");
    } else if(m_selectedTheme=="Tally Light") {
        assign(m_accent,"#8a5a00");assign(m_background,"#faf9f4");assign(m_darkBackground,"#f0efe9");
        assign(m_darkerBackground,"#e8e7df");assign(m_lighterBackground,"#dddbd0");
        assign(m_foreground,"#2c2c27");assign(m_darkForeground,"#74736a");assign(m_lightForeground,"#55544d");
        assign(m_muted,"#77766f");assign(m_selection,"#deddd4");assign(m_red,"#b7433f");
    } else {
        assign(m_accent,systemColor("accent","#FFD60A"));
        assign(m_background,systemColor("background","#202125"));
        assign(m_darkBackground,systemColor("dark_background","#101113"));
        assign(m_darkerBackground,systemColor("darker_background","#191a1d"));
        assign(m_lighterBackground,systemColor("lighter_background","#2b2c30"));
        assign(m_foreground,systemColor("foreground","#f1f0eb"));
        assign(m_darkForeground,systemColor("dark_foreground","#74767c"));
        assign(m_lightForeground,systemColor("light_foreground","#a0a1a6"));
        assign(m_muted,systemColor("muted",systemColor("dark_foreground","#a0a1a6")));
        assign(m_selection,systemColor("selection","#2b2c30"));
        assign(m_red,systemColor("red","#ff837b"));
    }
    const bool light=m_selectedTheme=="Tally Light" || (m_selectedTheme=="Follow system" && systemLight);
    if(m_lightMode!=light){m_lightMode=light;didChange=true;}
    if(!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
        QProcess p;p.start(QStandardPaths::findExecutable("hyprctl"),{"-j","getoption","decoration:rounding"});
        if(p.waitForFinished(250)){auto json=QJsonDocument::fromJson(p.readAllStandardOutput()).object();int r=json.value("int").toInt();if(r!=m_radius){m_radius=r;didChange=true;}}
    }
    if(didChange)emit changed();
}

void Theme::setTheme(const QString &name) {
    if(!themeOptions().contains(name) || m_selectedTheme==name)return;
    m_selectedTheme=name;
    QSettings settings("Tally","Tally");
    settings.setValue("appearance/theme",m_selectedTheme);
    reload();
    emit changed();
}
