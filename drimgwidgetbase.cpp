/***************************************************************************
 *   Copyright (C) 2006 by P. Putnik                                       *
 *   pp@ppest.org                                                          *
 *   new QT bindings by F. Heisig 2018                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "drimgwidgetbase.h"
#include "ui_drimgwidgetbase.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <qfileinfo.h>
#include <QMessageBox>
#include <QFileDialog>

//#include <klocale.h>
//#include <kfiledialog.h>

typedef unsigned char  byte;
typedef unsigned int   UINT;
typedef unsigned long  ULONG;

char act = 0;
char abortf = 0;

int  n, c, i, k, s, o;  //loop counter, general
char devStr[13];
char devStr1[9]  = "/dev/sda";
char devStr2[13] = "/dev/mmcblk0";
char dstr[60]; //for string operations
int  detCount;
char detDev[13][16] ;
char physd[13];
int  selected = 111; //set to non existing drive for begin
int  form = 0; //Format 0-raw, 1-hdf, 2-hdf256
int  Filesys = 0; // 0-unknown, 1-DOS, 2-PlusIDE, 3-PlusIDE 256 // 4-7 Charea ;  11-GemDos ; 13-FAT16 partition itself
char Swf=0 ; // Sub filesystem 1-Swapped High/Low
int  Fsub = 0;  // Swap request flag - by read or write drive
char loadedF[256];
char segflag = 0;

bool ov2ro = 1;

QString fileName;
FILE *finp, *fout, *ftout, *flout;
ULONG filelen;

ULONG f, g, m, u ; // general var
ULONG OffFS = 0 ;
ULONG exdl[16];
ULONG SecCnt = 1;
ULONG ChaSecCnt ;
ULONG fsecc, rest;
UINT  step, cyld ;
UINT  sectr = 32;
ULONG cylc  = 1 ;
UINT  heads = 8;  //CHS parameters

//Ramsoft hdf header template:
byte rsh[128] = {0x52,0x53,0x2d,0x49,0x44,0x45,0x1a,0x10,0,0x80,0,0,0,0,0,0,0,0,0,0,0,0,
   0x8a,0x84,0xd4,3, //last two is cyl count
   0,0,8,0,0,0,
   0,2,32,0,3,0,0, 0xd4,0,0,
   32,32,32,32,0x64,0x72,0x69,0x6d,32,32,0x6d,0x61,0x64,0x65,32,32,
   32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
   32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
   32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
   32,32,32,32,32,32,32,32,32,32,
   1,0,0,0,0,3,0,0,0,2,0,0} ;

unsigned char  bufr[640];
unsigned char  buf2[524];
unsigned char  bufsw[524] ;

QString op, qhm;

drimgwidgetbase::drimgwidgetbase(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::drimgwidgetbase)
{
    ui->setupUi(this);
    #ifdef FRANKS_DEBUG
    on_openIfButton_clicked();
    #endif
}

drimgwidgetbase::~drimgwidgetbase()
{
    delete ui;
}

void drimgwidgetbase::getCHS()
{
    qhm = ui->edSecTr->text() ;
    sectr = qhm.toUInt();
    qhm = ui->edHeads->text() ;
    heads = qhm.toUInt();
    qhm = ui->edCyl->text() ;
    cylc = qhm.toLong();
}

void drimgwidgetbase::getForm()
{
    if(ui->rawRB->isChecked())  form=0;
    if(ui->hdfRB->isChecked())  form=1;
    if(ui->h256RB->isChecked()) form=2;
}

void drimgwidgetbase::detSD()
{
 int drih, e;
 unsigned long long drisize;

#ifdef WINDOWS
 drih = _open(devStr,  O_RDONLY) ;
#else
 drih = open(devStr,  O_RDONLY | O_NONBLOCK) ;
#endif
 if (drih>0) {

   drisize = lseek64( drih, 0,  SEEK_END );
   fileclose(drih) ;


   if (drisize) {
     finp = fopen(devStr,"wb"); // CD ROMs will not open
     if (finp != NULL){
        exdl[detCount] = drisize/512; // Sector count
        c=1;
        qhm.setNum((double)drisize/1048576);
        strcat(dstr, (char*)qhm.toLatin1().data());
        strcat(dstr," MB");
        fclose(finp);
      }
    }
  }
  else{
    e = errno;
    if (e == EACCES) {
      QMessageBox::critical(this, "Drive open error.", "Need to be root for accessing devices. Root?", QMessageBox::Cancel, QMessageBox::Cancel);
      act=0;
    }
  }
}

void drimgwidgetbase::detSDloop()
{
    ui->listBox1->clear();
    detCount = 0;
    act      = 1;
    for (n=0;n<20;n++)
    {
        if (n<6) { devStr1[5] = 's' ;
            devStr1[7] = 'c'+n; //we're not checking /dev/sda and /dev/sdb
            strcpy(devStr,devStr1);
        }
        else if (n<12) { devStr1[5] = 'h' ;
            devStr1[7] = 'c'+n-6; //we're not checking /dev/hda and /dev/hdb
            strcpy(devStr,devStr1);
        }
        else {
            devStr2[11] = '0'+n-12; //but we're checking /dev/mmcblk0 to /dev/mmcblk7
            strcpy(devStr,devStr2);
        }

        strcpy(dstr,devStr);
        strcat(dstr,"  ");
        c=0; // hit flag

        detSD() ;
        if (act == 0) { break; }

        if (c) {
        ui->listBox1->addItem(QString(dstr));
        if (n<12){
          for (k=0;k<9;k++) detDev[k][detCount]=devStr[k]; // Store dev path
          for (k=9;k<13;k++) detDev[k][detCount]='\0';
        }
        else{
          for (k=0;k<13;k++) detDev[k][detCount]=devStr[k]; // Store dev path
        }
        detCount++;
      }
    }
    act = 0;
}

void drimgwidgetbase::on_refrButton_clicked()
{
    detSDloop() ; // Detect SCSI (USB) drives attached
/*
    ui->listBox1->addItem(QString("--- item 1 ---"));
    ui->listBox1->addItem(QString("--- item 2 ---"));
    ui->listBox1->addItem(QString("--- item 3 ---"));
*/
}



void drimgwidgetbase::on_quitButton_clicked()
{
    qApp->quit();
}

void drimgwidgetbase::on_listBox1_clicked(const QModelIndex &index)
{
    selected = index.row();

    qhm.setNum(selected);
    qhm.insert(0, "Drive: ");
    ui->inf1Label->setText(qhm); //Info about selected drive - better write here /dev...
    ui->inf2Label->setText(" ");

    SecCnt = exdl[selected];
    ui->inf3Label->setText(" ");   //Clear info line 3

    // Dev descriptor: TODO
      // Insert in EDsecC field SecCnt :

    qhm.setNum(SecCnt);
    ui->seccnEdit->setText(qhm);

    //ltoa(SecCnt,khm,10);
    //SetDlgItemText(hDlgWnd, EDsecC, khm);
    //Insert by sector count to edit boxes:

    UINT secw;
    sectr = 32;
    secw  = (SecCnt-1000)/512 ;
    heads = 2;
    if (secw>64)   heads = 4 ;
    if (secw>128)  heads = 8;
    if (secw>512)  heads = 16;
    if (secw>1024) heads = 32;
    cylc = SecCnt/(heads*sectr) ; //Sector per track is fixed to 32
    //Get sector #0 and see filesystem on drive:
       // Clear buffer ahead:
    for (n=0;n<512;n++) bufr[n] = 0 ;
    for (k=0;k<13;k++) physd[k] = detDev[k][selected];

    ui->inf4Label->setText(physd); //testing!!!
    finp = fopen(physd,"rb");
    if (finp == NULL) {
      QMessageBox::critical(this, "Drive open error.", "Need to be root for accessing devices. Root?", QMessageBox::Cancel, QMessageBox::Cancel);
      act=0; return;
    }

    fread(bufr,1,512,finp);
    fclose(finp);
    OffFS = 0 ;
    ui->secofEdit->setText("0");
    // Detecting file(partition) system used:
    Filesys = 0;
    Swf = 0 ;

    char PLUSIIS[]= "PLUSIDEDOS      ";
    char OSID[16];
    int matc = 0;
    //Get first 16 char:
    for( n = 0; n < 16; n++ ) OSID[n]=bufr[n];
    for( n = 0; n < 16; n++ ) {if (OSID[n]==PLUSIIS[n]) matc++ ;}
    if (matc == 16) {
        ui->inf2Label->setText(PLUSIIS);
        //SetDlgItemText(hDlgWnd, Sta2, PLUSIIS);
          //Get CHS from disk's boot sector:
        heads = bufr[34];
        sectr = bufr[35];
        cylc = bufr[32]+256*bufr[33];
    }
    //Check is 8-bit IDEDOS:
    matc = 0;
    for( n = 0; n < 16; n++ ) OSID[n]=bufr[n*2];
    for( n = 0; n < 16; n++ ) {if (OSID[n]==PLUSIIS[n]) matc++ ;}
    if (matc == 16) {
        ui->inf2Label->setText("PLUSIDEDOS 256 byte sect");
        heads = bufr[68];
        sectr = bufr[70];
        cylc = bufr[64]+256*bufr[66];
        // Check Hdf256:
        ui->h256RB->setChecked(1);
        ui->hdfRB->setChecked(0);
        ui->rawRB->setChecked(0);
    }
    //Put CHS values in ED boxes
    qhm.setNum(sectr);
    ui->edSecTr->setText(qhm);
    qhm.setNum(heads);
    ui->edHeads->setText( qhm );
    qhm.setNum(cylc);
    ui->edCyl->setText( qhm );

    // Check for GemDos:
    if (  ((bufr[0x1c7] == 'G') && (bufr[0x1c8] == 'E') && (bufr[0x1c9] == 'M'))
        ||((bufr[0x1c7] == 'X') && (bufr[0x1c8] == 'G') && (bufr[0x1c9] == 'M'))
        ||((bufr[0x1c7] == 'B') && (bufr[0x1c8] == 'B') && (bufr[0x1c9] == 'M')))
    {
        ui->inf2Label->setText("GemDos");
        Filesys = 11; Swf = 0;
    }
    // Check for GemDos by swapped H/L :
    for (n=0;n<512;n=n+2) { bufsw[n] = bufr[n+1];  bufsw[n+1] = bufr[n]; }
    if (  ((bufsw[0x1c7] == 'G') && (bufsw[0x1c8] == 'E') && (bufsw[0x1c9] == 'M'))
        ||((bufsw[0x1c7] == 'X') && (bufsw[0x1c8] == 'G') && (bufsw[0x1c9] == 'M'))
        ||((bufsw[0x1c7] == 'B') && (bufsw[0x1c8] == 'B') && (bufsw[0x1c9] == 'M')))
    {
        ui->inf2Label->setText("GemDos SW");
        Filesys = 11; Swf = 1;
    }
    // See is FAT16 partition:
    if (bufr[21] == 0xF8) {
       ui->inf2Label->setText("FAT16 part.");
       Filesys = 13;
    }
    else{
      //See is DOS mbr:
      if ((bufr[510] == 0x55) && (bufr[511] == 0xAA)){
         ui->inf2Label->setText("DOS mbr");
         Filesys = 1;
      }
      // Check for PP Charea doS
      if ((bufr[0] == 0x41) && (bufr[12] == 0x42) && (bufr[24] == 0x43)){
         strcpy(dstr,"Charea DoS");
         // Check is 8-bit simple IF drive/media:
         if ((bufr[0x1BC] == 'J') && (bufr[0x1BE] == 'R')){
            strcat(dstr," 8-bit");
            // Check Hdf256:
            ui->h256RB->setChecked(1);
            ui->hdfRB->setChecked(0);
            ui->rawRB->setChecked(0);
         }
         if ((bufr[448] == 'B') && (bufr[450] == 'S')){ strcat(dstr, " BigSect."); }
         ui->inf2Label->setText(dstr);
      }
    }
    ui->inf6Label->setText(" "); // Clear filename info
}

void drimgwidgetbase::on_FileTrButton_clicked()
{
    //Filesys = 1 ; //Testing!!!!
   if (act)  { return; }
   getForm();
   getCHS();
   segflag = 0;
   if ((Filesys == 11) || (Filesys == 13) || (Filesys == 1)) {
      gem = new GemdDlg(this);
      gem->exec();
   }
   else {
      QMessageBox::information(this, "Not implemented yet", "Not implemented yet.",QMessageBox::Cancel, QMessageBox::Cancel);
      /*
      transferZ().exec() ;  //ZX Charea dlg call
      if (geohit) {
         qhm.setNum(sectr);
         edSecTr->setText(qhm);
         qhm.setNum(heads);
         edHeads->setText( qhm );
         cylc = SecCnt/(sectr*heads) ;
         qhm.setNum(cylc);
         edCyl->setText( qhm );
      }
      */
   }
   if (segflag){
      ui->inf3Label->setText(" "); // Clear selected sec count message
      qhm.setNum(OffFS);
      ui->secofEdit->setText(qhm);
      qhm.setNum(ChaSecCnt);
      ui->seccnEdit->setText(qhm);
   }
    // correct CHS for case of successfull geo guess:
}


void drimgwidgetbase::on_readButton_clicked()
{
    if (act) return;
    act = 1;
    //Set CHS according to Edit boxes first:
    getCHS();
    //Get OffSet and Sector count:
    qhm = ui->secofEdit->text() ;
    OffFS = qhm.toLong();
    qhm = ui->seccnEdit->text() ;
    SecCnt = qhm.toLong();
    qhm.setNum(SecCnt);
    qhm.insert(0,"Sector count: ");
    ui->inf3Label->setText(qhm);

    getForm(); //Set format flag according to RB-s
    if ( selected == 99 ) { //Special case - hdf to raw or vice versa
        if ( (form==0) & (Fsub) ) // Swap L/H and save in new file
        {
            //fileName = KFileDialog::getSaveFileName(i18n("~"), i18n("*.raw|Raw image files\n*.img|Img image files\n*.*|All files"), this, i18n("File to save"));
            fileName = QFileDialog::getSaveFileName(this, "File to save", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)");
            if( !fileName.isEmpty() )  {
                QCoreApplication::processEvents();
                int  status=0;
                ULONG copied = 0;

                finp = fopen(loadedF,"rb");
                if (finp == NULL) {
                   QMessageBox::critical(this, "File open error.", "Input file open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
                   act=0; return;
                }
                fout = fopen((char*)fileName.toLatin1().data(),"wb");
                if (fout == NULL) {
                   QMessageBox::critical(this, "File open error.", "Output file open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
                   fclose(finp);
                   act=0; return;
                }
                ui->curopLabel->setText("Swapping L-H...");
                fseek(finp, 0, SEEK_END) ;
                ULONG filelen = ftell(finp); //Filelength got
                fseek(finp, 0, SEEK_SET) ;
                SecCnt = filelen/512 ;

                //ULONG m;
                int prog = 0;
                int prnxt = 1;

                ui->progressBar1->setValue(0);
                setCursor(QCursor(Qt::WaitCursor));

                for( m = 0; m < SecCnt; m++ ) {
                   status=fread(bufr,1,512,finp);
                   for (n = 0; n<512; n=n+2) { buf2[n] = bufr[n+1]; buf2[n+1] = bufr[n] ; }
                   status = fwrite(buf2,1,512,fout);
                   QCoreApplication::processEvents();
                   copied = copied+status;
                   if (abortf) { abortf=0 ; break; }
                   prog = 100*m/SecCnt;
                   if (prog>prnxt) {
                      ui->progressBar1->setValue(prog+1);
                      prnxt = prnxt+1 ;
                   }
                }
                fclose(finp);
                fclose(fout);
                setCursor(QCursor(Qt::ArrowCursor));
                //Print out info about data transfer:
                qhm.setNum(m);
                qhm.append(" sectors swapped L-H into file of ");
                QString num;
                num.setNum(copied);
                qhm.append(num);
                qhm.append(" bytes.");
                ui->curopLabel->setText(qhm);
            }
        }
        else {
           if (form > 0) {  // contra selection here - to make conversion
              fileName = QFileDialog::getSaveFileName(this, "File to save", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)");  }
           else {
              fileName = QFileDialog::getSaveFileName(this, "File to save", NULL, "Hdf image files (*.hdf);;All files (*.*)"); }
           if(!fileName.isEmpty()){
              QCoreApplication::processEvents();
              int   status = 0;
              ULONG copied = 0;
              ULONG cophh  = 0;
              finp = fopen(loadedF,"rb");
              if (finp == NULL) {
                 QMessageBox::critical(this, "File open error.", "Input file open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
                 act=0; return;
              }
              fout = fopen((char*)fileName.toLatin1().data(),"wb");
              if (fout == NULL) {
                 QMessageBox::critical(this, "File open error.", "Output file open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
                 fclose(finp);
                 act=0; return;
              }
              //Message at bottom
              ui->curopLabel->setText("File conversion...");
              //SetDlgItemText(hDlgWnd, StaRWinf, "File conversion... Right click to abort");
              if (form == 0) {  //if hdf wanted
                 //Insert CHS in hdf header:
                 rsh[28] = heads;
                 rsh[24] = cylc;
                 rsh[25] = cylc>>8;
                 rsh[34] = sectr ;
                 // if (form == 2) { //Hdf 256
                 //   rsh[8] = 1; }
                 // else   // no hdf 256 support here
                 rsh[8] = 0;
                 for( n = 0; n < 128; n++ ) {
                    fputc(rsh[n],fout);
                 }
                 cophh = 128;
              }
              else{
                 fseek(finp,128,0) ; // just skip hdf header
              }
              int prog = 0;
              int prnxt = 1;
              ui->progressBar1->setValue(0);
              //SendMessage(hPb, PBM_SETPOS, 0, 0);if (!act) {
              for( m = 0; m < SecCnt; m++ ) {
                 setCursor(QCursor(Qt::WaitCursor));
                 //SetCursor( jc) ;
                 status=fread(bufr,1,512,finp);
                 // if (form == 2) {
                 //    for (n = 0; n<256; n++) buf2[n] = bufr[n*2];
                 //    status = fwrite(buf2,1,256,fout);
                 //    }
                 //else
                 status = fwrite(bufr,1,512,fout);
                 QCoreApplication::processEvents();
                 copied = copied + status;
                 if (abortf) { abortf=0 ; break; }
                 prog = 100*m/SecCnt;
                 if (prog>prnxt) {
                    ui->progressBar1->setValue(prog+1);
                    prnxt = prnxt+1 ;
                 }
              }
              fclose(finp);
              fclose(fout);
              setCursor(QCursor(Qt::ArrowCursor));
              //Print out info about data transfer:
              qhm.setNum(m);
              qhm.append(" sectors copied from img. file into file of ");
              QString num;
              num.setNum(copied + cophh);
              qhm.append(num);
              qhm.append(" bytes.");
              ui->curopLabel->setText(qhm);
           } // filesel success end
       } // else after Fsub if end
   } // vice versa end
    // Here comes read from drives, medias...:
   else{
        if (form == 0) {  // contra selection here - to make conversion
           fileName = QFileDialog::getSaveFileName(this, "File to save", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)"); }
        else {
           fileName = QFileDialog::getSaveFileName(this, "File to save", NULL, "Hdf image files (*.hdf);;All files (*.*)"); }
        if( !fileName.isEmpty() )  {
            QCoreApplication::processEvents();
            int  status=0;
            ULONG copied = 0;
            ULONG cophh = 0;

            for (k=0;k<9;k++){ physd[k] = detDev[k][selected]; }
            ui->inf4Label->setText(physd); //testing!!!
            finp = fopen(physd,"rb");
            if (finp == NULL) {
                QMessageBox::critical(this, "File open error.", "Input file open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
                act=0; return;
            }
            fout = fopen((char*)fileName.toLatin1().data(),"wb");
            if (fout == NULL) {
               QMessageBox::critical(this, "File open error.", "Output file open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
               fclose(finp);
               act=0; return;
            }
            ui->curopLabel->setText("Reading from drive...");
            if (form >0) {  //if hdf wanted
              //Insert CHS in hdf header:
              rsh[28] = heads;
              rsh[24] = cylc;
              rsh[25] = cylc>>8;
              rsh[34] = sectr ;
              rsh[8]  = (form == 2) ? 1 : 0;
              for( n = 0; n < 128; n++ ) {
                 fputc(rsh[n],fout);
              }
              cophh = 128;
            }
            int prog = 0;
            int prnxt = 1;
            fseek(finp, OffFS*512, SEEK_SET);
            ui->progressBar1->setValue(0);
            setCursor(QCursor(Qt::WaitCursor));
            //return;
            for( m = 0; m < SecCnt; m++ ) {
               status=fread(bufr,1,512,finp);
               if (form == 2) {
                  for (n = 0; n<256; n++) buf2[n] = bufr[n*2];
                  status = fwrite(buf2,1,256,fout);
               }
               else {
                  if (Fsub){
                    // Swap L/H
                    for (n = 0; n<512; n=n+2) { buf2[n] = bufr[n+1]; buf2[n+1] = bufr[n] ; }
                    status = fwrite(buf2,1,512,fout);
                  }
                  else{
                    status = fwrite(bufr,1,512,fout);
                  }
               }
               copied = copied + status;
               QCoreApplication::processEvents();
               if (abortf) { abortf=0 ; break; }
               prog = 100*m/SecCnt;
               if (prog>prnxt) {
                  ui->progressBar1->setValue(prog+1);
                  prnxt = prnxt + 1;
               }
            }
            fclose(finp);
            fclose(fout);
            setCursor(QCursor(Qt::ArrowCursor));

            qhm.setNum(m);
            qhm.append(" sectors copied from img. file into file of ");
            QString num;
            num.setNum(copied + cophh);
            qhm.append(num);
            qhm.append(" bytes.");
            ui->curopLabel->setText(qhm);
        }
    }
    act=0;
}

void drimgwidgetbase::on_openIfButton_clicked()
{
    if (act) { return; }
    #ifdef FRANKS_DEBUG
    {
      #ifdef WINDOWS
      fout = fopen("D:\\Projekte\\tools\\DrImg\\testimage.img","rb");
      #else
      fout = fopen("/home/frank/Projekte/ATARI/DrImg/testimage.img","rb");
      #endif
    #else
    getForm();
    if (form == 0) {
        fileName = QFileDialog::getOpenFileName(this, "File to load", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)"); }
    else {
        fileName = QFileDialog::getOpenFileName(this, "File to load", NULL, "Hdf image files (*.hdf);;All files (*.*)"); }
    if( !fileName.isEmpty() )
    {
      fout = fopen((char*)fileName.toLatin1().data(),"rb");
    #endif
      if (fout == NULL) {
          QMessageBox::critical(this, "File open error.", "File open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
          return;
      }
      //Load 512+128 bytes
      //status=
      fread(bufr,1,640,fout);
      // get filelen
      fseek(fout, 0, SEEK_END) ;
      filelen = ftell(fout); //Filelength got
      fclose(fout);
      #ifdef FRANKS_DEBUG
      #ifdef WINDOWS
      strcpy(loadedF, "D:\\Projekte\\tools\\DrImg\\testimage.img");
      #else
      strcpy(loadedF, "/home/frank/Projekte/ATARI/DrImg/testimage.img");
      #endif
      #else
      strcpy(loadedF, (char*)fileName.toLatin1().data());
      #endif

      ui->inf1Label->setText( "File selected" );
      selected = 99; //flag
      // Get here CHS from hdf header if...
      int ofset = 0;
      if (form > 0) ofset = 128;
      if (form>0) {
          heads = bufr[28];
          sectr = bufr[34];
          cylc = 256*bufr[25]+bufr[24] ;
          SecCnt = (filelen-ofset)/512 ;
          if ( bufr[8] == 1 ) SecCnt = (filelen-ofset)/256 ;
      }
      else { heads = 8;
          sectr = 32;
          SecCnt = filelen/512 ;
          cylc = SecCnt/(sectr*heads) ;
      }
      qhm.setNum(sectr);
      ui->edSecTr->setText(qhm);
      qhm.setNum(heads);
      ui->edHeads->setText(qhm);
      qhm.setNum(cylc);
      ui->edCyl->setText(qhm);
      qhm.setNum(SecCnt);
      ui->seccnEdit->setText(qhm);
      ui->inf5Label->setText(" ");  // Clear
      ui->secofEdit->setText( "0" );

      OffFS   = 0 ;
      Filesys = 0;
      Swf     = 0 ;
      char PLUSIIS[]= "PLUSIDEDOS      ";
      char OSID[16];
      int matc = 0;
      //Get first 16 char:
      for( n = 0 ; n < 16; n++ ) { OSID[n]=bufr[n+ofset]; }
      for( n = 0; n < 16; n++ ) {if (OSID[n]==PLUSIIS[n]) { matc++; }}

      if (matc == 16) {
      //Check is 8-bit IDEDOS:
         if ((form>0) && ( bufr[8] == 1 )){
            ui->inf2Label->setText( "PLUSIDEDOS 256 byte sect");
            goto sethdf256 ;
         }
         else {
            ui->inf2Label->setText( PLUSIIS);
            goto sethdf512 ;
         }
      }
      // Check for GemDos:

      if (  ((bufr[0x1c7] == 'G') && (bufr[0x1c8] == 'E') && (bufr[0x1c9] == 'M'))
          ||((bufr[0x1c7] == 'X') && (bufr[0x1c8] == 'G') && (bufr[0x1c9] == 'M'))
          ||((bufr[0x1c7] == 'B') && (bufr[0x1c8] == 'G') && (bufr[0x1c9] == 'M'))){
         ui->inf2Label->setText("GemDos");
         Filesys = 11 ;
      }
      // Check for GemDos by swapped H/L :
      for (n=0;n<512;n=n+2) { bufsw[n] = bufr[n+1];  bufsw[n+1] = bufr[n]; }
      if (  ((bufsw[0x1c7] == 'G') && (bufsw[0x1c8] == 'E') && (bufsw[0x1c9] == 'M'))
          ||((bufsw[0x1c7] == 'X') && (bufsw[0x1c8] == 'G') && (bufsw[0x1c9] == 'M'))
          ||((bufsw[0x1c7] == 'B') && (bufsw[0x1c8] == 'G') && (bufsw[0x1c9] == 'M'))){
         ui->inf2Label->setText("GemDos SW");
         Filesys = 11;
         Swf = 1;
      }
      // See is FAT16 partition:
      if (bufr[21] == 0xF8) {
         ui->inf2Label->setText("FAT16 part.");
         Filesys = 13;
         goto removh;
      }
      //See is DOS mbr:
      if ((bufr[510+ofset] == 0x55) && (bufr[511+ofset] == 0xAA)){
         ui->inf2Label->setText( "DOS mbr");
         Filesys = 1 ;
         goto removh ;
      }
      // Check for PP Charea doS
      if (( bufr[0+ofset] == 0x41 )  && ( bufr[12+ofset] == 0x42 )  &&  ( bufr[24+ofset] == 0x43 ))  {  //ABC
         strcpy(dstr, "Charea DoS 16-bit");
         if (( bufr[ofset+448] == 'B' ) && ( bufr[ofset+450] == 'S' )) { strcat(dstr," BigSect."); }
         ui->inf2Label->setText( dstr);
         goto sethdf512 ;
      }
      if (( bufr[0+ofset] == 0x41 ) && ( bufr[6+ofset] == 0x42 )  && ( bufr[12+ofset] == 0x43 )) { //ABC
         strcpy(dstr,"Charea DoS 8-bit");
         if (( bufr[ofset+224] == 'B' ) && ( bufr[ofset+225] == 'S' )) { strcat(dstr," BigSect."); }
         ui->inf2Label->setText( dstr);
         goto sethdf256 ;
      }
      if ( form >0 ) {
        if ( bufr[8] == 1 ){
           goto sethdf256;
         }
         else{
            goto sethdf512;
         }
      }
      goto removh;

sethdf256:
      ui->h256RB->setChecked(1);
      ui->hdfRB->setChecked(0);
      goto removh ;

sethdf512:
      if ( form ) {
         ui->h256RB->setChecked(0);
         ui->hdfRB->setChecked(1);
      }

removh:
      ui->listBox1->clearSelection();
      //  SendMessage(GetDlgItem(hDlgWnd, ListBox), LB_SETCURSEL, -1, 1) ; // remove highlight of drive selection
      qhm.setNum(SecCnt);
      qhm.insert(0, "Sector count: ") ;
      ui->inf3Label->setText(qhm);
      //Print out loaded filename:
      ui->inf6Label->setText(fileName );
   }
}

void drimgwidgetbase::on_writeButton_clicked()
{
   if (act) { return; }
   act = 1;
   getForm() ;
   if (form == 0) {
      fileName = QFileDialog::getOpenFileName(this, "File to write", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)"); }
   else {
      fileName = QFileDialog::getOpenFileName(this, "File to write", NULL, "Hdf image files (*.hdf);;All files (*.*)"); }
   if( !fileName.isEmpty() )
   {
      if( !fileName.isEmpty() )  {
         QCoreApplication::processEvents();
         if ( selected==99 ) { qhm = "Write to selected file!"; }
         else {
            qhm.setNum(selected);
            qhm.insert(0, "Write to selected drive ");
         }
         qhm.append("\nAre you sure?");
         // File to drive
         if (QMessageBox::question(this, "Writing", qhm, QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::No)
         { act=0; return; }
         int hDevice;            // handle to the drive or file for write on
         if ( selected==99) {
            hDevice = open(loadedF, O_WRONLY);
         }
         else{
            for (k=0; k<9; k++) physd[k] = detDev[k][selected];
            if (( ov2ro ) && ( SecCnt>2097152)) {
               QMessageBox::critical(this, "Read only.", "Read only for large drives!\nStop",QMessageBox::Cancel, QMessageBox::Cancel);
               act=0;
               return;
             }
             hDevice = open(physd, O_WRONLY); // | O_FSYNC );
         }
         if (hDevice <0){ // cannot open the drive
            QMessageBox::critical(this, "Drive open error.", "Drive open error.\nRoot?",QMessageBox::Cancel, QMessageBox::Cancel);
            act=0; return;
         }
         // Open image file for read:
         fout = fopen((char*)fileName.toLatin1().data(),"rb");
         if (fout == NULL) {
         //printf("File open error!\n");
            fileclose(hDevice);
            act=0; return ;
         }
         if ( selected==99) ui->curopLabel->setText("Writing to file...");
         else ui->curopLabel->setText("Writing to drive...");

         setCursor(QCursor(Qt::WaitCursor));
         ULONG cophh = 0;
         //SecCnt = exdl[selected];  //Already set **** - Maybe changed.... see it
         if (form > 0) {  //if hdf to load
            cophh = 128;
         }

         fseek(fout, 0, SEEK_END) ;
         ULONG filelen;
         filelen = ftell(fout); //Filelength got

         fseek(fout, cophh, SEEK_SET) ; // Set to 0 by raw, or to 128 by hdf
         if (form == 2)  fsecc = (filelen-cophh)/256 ;
         else fsecc = (filelen-cophh)/512 ;
         qhm = ui->seccnEdit->text() ;
         SecCnt = qhm.toLong();
         if (SecCnt < fsecc)  fsecc = SecCnt;
         qhm.setNum(fsecc);
         ui->inf4Label->setText(qhm) ; // testing!!!

         //DWORD bytesw;
         //ULONG m;
         int prog = 0;
         int prnxt = 1;
         int  status=0;
         ULONG copied = 0;

         //*** Need to get value from edLine!!!
         qhm = ui->secofEdit->text() ;
         OffFS = qhm.toLong();
         lseek(hDevice, OffFS*512, SEEK_SET );
         ui->progressBar1->setValue(0);
         for( m = 0; m < fsecc; m++ ) {
            if (form == 2) {
               status=fread(buf2,1,256,fout);
               if (status<256) break;
               for (n = 0; n<256; n++)  bufr[n*2] = buf2[n];
            }
            else {
               status=fread(bufr,1,512,fout);
               if (status<512) break;
            }
            if (Fsub) {
               // Swap L/H
               for (n = 0; n<512; n=n+2) { buf2[n] = bufr[n+1]; buf2[n+1] = bufr[n] ; }
               copied = copied + write(hDevice,buf2,512) ;
            }
            else{
               copied = copied+write(hDevice,bufr,512);
            }
            QCoreApplication::processEvents();
            if (abortf) { abortf=0 ; break; }
            prog = 100*m/fsecc;
            if (prog>prnxt) {
               // Workaround because of delayed write
               // SYNC mode is too slow
               if (selected!=99){
                  n = fileclose(hDevice);
                  hDevice = open(physd, O_WRONLY ); // To ensure correct progress bar when writing to Flash media
               }
               lseek(hDevice, OffFS*512+copied, SEEK_SET ); // restore pos
               ui->progressBar1->setValue(prog+1);
               prnxt = prnxt+1 ;
            }
         }
         fclose(fout);
         n = fileclose(hDevice);
         setCursor(QCursor(Qt::ArrowCursor));
         //Print out info about data transfer:
         qhm.setNum(copied);
         qhm.append(" bytes (");
         QString num;
         num.setNum(m);
         qhm.append(num);
         if (selected==99) qhm.append(" sectors ) written to file.");
         else qhm.append(" sectors ) written to drive.");
         ui->curopLabel->setText(qhm);
      }
   }
   act = 0;
}
void drimgwidgetbase::on_abortButton_clicked()
{
   if (act) abortf = 1 ;
}

void drimgwidgetbase::on_creimfButton_clicked()
{
   int  status;
   int  secsize;

   if (act) { return; }
   act=1;

   getForm(); //Set format flag according to RB-s
   // Get CHS parameters from EDboxes:
   getCHS();    // CHS too
   SecCnt = sectr*heads*cylc;

   qhm.setNum(SecCnt);
   ui->seccnEdit->setText( qhm );

   //inf5Label->setText( " ");  // Clear
   OffFS = 0 ;
   ui->secofEdit->setText( "0" );

   // Open fileselector for giving name:
   if (form == 0){
      fileName = QFileDialog::getSaveFileName(this, tr("File to save"), NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)");
   }
   else {
      fileName = QFileDialog::getSaveFileName(this, tr("File to save"), NULL, "Hdf image files (*.hdf);;All files (*.*)");
   }
   if( !fileName.isEmpty() )  {
      //update() ;
      QCoreApplication::processEvents();
      //UpdateWindow(hDlgWnd);
      fout = fopen((char*)fileName.toLatin1().data(),"wb");
      if (fout == NULL) {
         act=0;
         return;
      }
      int prog = 0;
      int prnxt = 1;
      ui->curopLabel->setText( "Creating image file...");
      ui->progressBar1->setValue( 0 );
      setCursor(QCursor(Qt::WaitCursor));
      m = 0 ;
      if ( form > 0 ) { // HDF images
         // Set values in HDF header:
         rsh[28] = heads;
         rsh[24] = cylc;
         rsh[25] = cylc>>8;
         rsh[34] = sectr ;
         rsh[8] = (form == 2) ? 1 : 2; //Hdf 256
         status = fwrite(rsh,1,128,fout);
         if (status != 128) { /*todo*/ }
         m = m+128 ;
      }
      secsize = ( form == 2 ) ? 256 : 512;
      // Just make file with totalsectors*512 (or 256) 0 bytes
      //clear buffer:
      for (n=0;n<secsize;n++){ bufr[n] = 0 ; }
      g = sectr*heads*cylc ;
      for (f=0;f<g;f++) {
         status = fwrite(bufr,1,secsize,fout);
         if (status != secsize) { /*todo*/ }
         QCoreApplication::processEvents();
         m = m+secsize ;
         if (abortf) { abortf=0; break; }
         //u = m/secsize ;
         prog = (100*(m/secsize))/SecCnt;
         if (prog>prnxt) {
            ui->progressBar1->setValue( prog+1 );
            prnxt++ ;
         }
      }  // some check needed!
      //Close file:
      fclose(fout);
      setCursor(QCursor(Qt::ArrowCursor));
      for( n = 0; n < 60; n++ ) dstr[n] = 0; //Clear string
       qhm.setNum(m);
       qhm.insert(0, "Image file of ");
       qhm.append(" bytes created.");
       ui->curopLabel->setText(qhm);
   } // if filesel end
   act=0;
}

void drimgwidgetbase::on_ov2roCB_clicked()
{
   // Set flag according to CB state:
   if( ui->ov2roCB->isChecked() ) ov2ro = 1;
   else ov2ro = 0 ;
}

void drimgwidgetbase::on_swapCB_clicked()
{
   if( ui->swapCB->isChecked() ) Fsub = 1;
   else Fsub = 0 ;
}

