#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString darkBackground READ darkBackground NOTIFY changed)
    Q_PROPERTY(QString darkerBackground READ darkerBackground NOTIFY changed)
    Q_PROPERTY(QString lighterBackground READ lighterBackground NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString darkForeground READ darkForeground NOTIFY changed)
    Q_PROPERTY(QString lightForeground READ lightForeground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString red READ red NOTIFY changed)
    Q_PROPERTY(bool lightMode READ lightMode NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    Q_PROPERTY(QStringList themeOptions READ themeOptions CONSTANT)
    Q_PROPERTY(QString selectedTheme READ selectedTheme NOTIFY changed)
public:
    explicit Theme(QObject *parent=nullptr);
    QString accent() const { return m_accent; }
    QString background() const { return m_background; }
    QString darkBackground() const { return m_darkBackground; }
    QString darkerBackground() const { return m_darkerBackground; }
    QString lighterBackground() const { return m_lighterBackground; }
    QString foreground() const { return m_foreground; }
    QString darkForeground() const { return m_darkForeground; }
    QString lightForeground() const { return m_lightForeground; }
    QString muted() const { return m_muted; }
    QString selection() const { return m_selection; }
    QString red() const { return m_red; }
    bool lightMode() const { return m_lightMode; }
    int radius() const { return m_radius; }
    QStringList themeOptions() const { return {"Follow system", "Tally Dark", "Tally Light"}; }
    QString selectedTheme() const { return m_selectedTheme; }
    Q_INVOKABLE void setTheme(const QString &name);
signals: void changed();
private slots: void reload();
private:
    QString m_dir;
    QString m_accent="#FFD60A", m_background="#202125", m_darkBackground="#101113";
    QString m_darkerBackground="#191a1d", m_lighterBackground="#2b2c30";
    QString m_foreground="#f1f0eb", m_darkForeground="#74767c", m_lightForeground="#a0a1a6";
    QString m_muted="#a0a1a6", m_selection="#2b2c30", m_red="#ff837b";
    QString m_selectedTheme="Follow system";
    bool m_lightMode=false;
    int m_radius=0;
    QFileSystemWatcher m_watcher; QTimer m_timer;
};
