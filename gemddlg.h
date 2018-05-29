#ifndef GEMDDLG_H
#define GEMDDLG_H

#include <QDialog>
#include <QModelIndex>

namespace Ui {
class GemdDlg;
}

class GemdDlg : public QDialog
{
    Q_OBJECT

public:
    explicit GemdDlg(QWidget *parent = 0);
    ~GemdDlg();

    void loadroot();
    void ShowUF();
    void opensubd(int index);
    void subdirh();
    void subdl();

private slots:
    void on_partLB_clicked(const QModelIndex &index);
    void on_filesLB_doubleClicked(const QModelIndex &index);
    void on_opdirP_clicked();
    void on_dirupP_clicked();
    void on_setddP_clicked();
    void on_extrP_clicked();
    void on_timeCB_clicked();
    void on_addfP_clicked();

    void on_newfP_clicked();

    void on_desallP_clicked();

    void on_quitP_clicked();

private:
    Ui::GemdDlg *ui;

    void OpenDialog();
};

int fileopen(const char* fname, const unsigned long flags);
int fileclose(int fd);

#endif // GEMDDLG_H
