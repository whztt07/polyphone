#ifndef CONFIGSECTIONREPOSITORY_H
#define CONFIGSECTIONREPOSITORY_H

#include <QWidget>
#include "usermanager.h"

namespace Ui {
class ConfigSectionRepository;
}

class ConfigSectionRepository : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigSectionRepository(QWidget *parent = 0);
    ~ConfigSectionRepository();
    void initialize();

private slots:
    void onConnectionStateChanged(UserManager::ConnectionState connectionState);

    void on_lineUser_editingFinished();
    void on_linePassword_editingFinished();
    void on_buttonCreateAccount_clicked();
    void on_buttonBecomePremium_clicked();

    void on_pushConnect_clicked();

private:
    Ui::ConfigSectionRepository *ui;
    UserManager::ConnectionState _currentState;
};

#endif // CONFIGSECTIONREPOSITORY_H
