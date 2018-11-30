/***************************************************************************
**                                                                        **
**  Polyphone, a soundfont editor                                         **
**  Copyright (C) 2013-2018 Davy Triponney                                **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program. If not, see http://www.gnu.org/licenses/.    **
**                                                                        **
****************************************************************************
**           Author: Davy Triponney                                       **
**  Website/Contact: https://www.polyphone-soundfonts.com                 **
**             Date: 01.01.2013                                           **
***************************************************************************/

#include "downloadprogresscell.h"
#include "ui_downloadprogresscell.h"
#include "contextmanager.h"
#include "downloadmanager.h"

DownloadProgressCell::DownloadProgressCell(QString soundfontName, int soundfontId, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DownloadProgressCell),
    _soundfontName(soundfontName),
    _soundfontId(soundfontId)
{
    ui->setupUi(this);

    // Style
    ui->pushCancel->setIcon(ContextManager::theme()->getColoredSvg(":/icons/close.svg", QSize(16, 16), ThemeManager::LIST_TEXT));
    ui->pushOpen->setIcon(ContextManager::theme()->getColoredSvg(":/icons/document-open.svg", QSize(16, 16), ThemeManager::LIST_TEXT));
    ui->pushOpen->hide();

    // Data
    ui->labelTitle->setText(_soundfontName);
}

DownloadProgressCell::~DownloadProgressCell()
{
    delete ui;
}

void DownloadProgressCell::progressChanged(int percent, QString finalFileName)
{
    _filename = finalFileName;
    ui->labelPercent->setText(QString::number(percent) + "%");

    if (finalFileName != "")
    {
        ui->pushOpen->setToolTip(trUtf8("Open \"%0\"").arg(_filename));
        ui->pushCancel->hide();
        ui->pushOpen->show();
    }
}

void DownloadProgressCell::on_pushOpen_clicked()
{

}

void DownloadProgressCell::on_pushCancel_clicked()
{
    DownloadManager::getInstance()->cancel(_soundfontId);
}