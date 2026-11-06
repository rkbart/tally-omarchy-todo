#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
public:
    explicit Theme(QObject *parent=nullptr);
    QString accent() const { return m_accent; }
    int radius() const { return m_radius; }
signals: void changed();
private slots: void reload();
private:
    QString m_dir, m_accent="#FFD60A"; int m_radius=0;
    QFileSystemWatcher m_watcher; QTimer m_timer;
};
