/***************************************************************************
**                                                                        **
**  Polyphone, a soundfont editor                                         **
**  Copyright (C) 2013 Davy Triponney                                     **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Davy Triponney                                       **
**  Website/Contact: http://www.polyphone.fr/                             **
**             Date: 01.01.2013                                           **
***************************************************************************/

#include "import_sfz.h"
#include "pile_sf2.h"
#include "config.h"

ImportSfz::ImportSfz(Pile_sf2 * sf2) :
    _sf2(sf2),
    _currentBloc(BLOC_UNKNOWN)
{
}

void ImportSfz::import(QString fileName, int &numSf2)
{
    QFile inputFile(fileName);
    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);
        while (!in.atEnd())
        {
            // Lecture ligne par ligne
            QString line = in.readLine();

            // Suppression des commentaires
            if (line.contains("//"))
                line = line.left(line.indexOf("//"));

            // Découpage
            QStringList list = line.split(" ", QString::SkipEmptyParts);
            int length = list.size();
            for (int i = length - 1; i >= 1; i--)
            {
                if (!list.at(i).contains(QRegExp("[<>=]")))
                {
                    list[i-1] += " " + list[i];
                    list.removeAt(i);
                }
            }

            // Stockage
            for (int i = 0; i < list.size(); i++)
            {
                QString str = list.at(i);

                // Valide ?
                if (str.size() > 2)
                {
                    if (str.left(1) == "<" && str.right(1) == ">")
                        changeBloc(str.right(str.size()-1).left(str.size()-2));
                    else if (str.contains("="))
                    {
                        int index = str.indexOf("=");
                        QString opcode = str.left(index).toLower();
                        QString value = str.right(str.length() - index - 1);
                        if (opcode.size() && value.size())
                            addOpcode(opcode, value);
                    }
                }
            }
        }
        inputFile.close();

        // Les offsets doivent se trouver à côté des samples, pas dans les divisions globales
        // On ne garde que les filtres supportés par le format sf2
        for (int i = 0; i < _listeEnsembles.size(); i++)
        {
            _listeEnsembles[i].moveOpcodeInSamples(Parametre::op_offset);
            _listeEnsembles[i].moveOpcodeInSamples(Parametre::op_end);
            _listeEnsembles[i].moveOpcodeInSamples(Parametre::op_loop_start);
            _listeEnsembles[i].moveOpcodeInSamples(Parametre::op_loop_end);
            _listeEnsembles[i].checkFilter();
        }

        // Création d'un sf2
        int numBank = 0;
        int numPreset = 0;
        QString nom = getNomInstrument(fileName, numBank, numPreset);
        EltID idSf2(elementSf2, -1, -1, -1, -1);
        if (numSf2 == -1)
        {
            idSf2.indexSf2 = _sf2->add(idSf2, false);
            numSf2 = idSf2.indexSf2;
            _sf2->set(idSf2, champ_name, nom, false);
        }
        else
        {
            idSf2.indexSf2 = numSf2;
            _sf2->set(idSf2, champ_name, QObject::trUtf8("Import sfz"), false);
        }
        _sf2->closestAvailablePreset(idSf2, numBank, numPreset);

        // Création d'un preset
        EltID idPrst = idSf2;
        idPrst.typeElement = elementPrst;
        idPrst.indexElt = _sf2->add(idPrst, false);
        _sf2->set(idPrst, champ_name, nom, false);
        Valeur val;
        val.wValue = numBank;
        _sf2->set(idPrst, champ_wBank, val, false);
        val.wValue = numPreset;
        _sf2->set(idPrst, champ_wPreset, val, false);

        // Création des instruments
        EltID idInst = idSf2;
        idInst.typeElement = elementInst;
        for (int i = 0; i < _listeEnsembles.size(); i++)
        {
            idInst.indexElt = _sf2->add(idInst, false);
            QString nomInst = nom;
            if (_listeEnsembles.size() > 10)
                nomInst = nom.left(17) + "-" + QString::number(i);
            else if (_listeEnsembles.size() > 1)
                nomInst = nom.left(18) + "-" + QString::number(i);
            else
                nomInst = nom;
            _sf2->set(idInst, champ_name, nomInst, false);

            EltID idInstSmpl = idInst;
            idInstSmpl.typeElement = elementInstSmpl;

            // Lien dans le preset
            EltID idPrstInst = idPrst;
            idPrstInst.typeElement = elementPrstInst;
            idPrstInst.indexElt2 = _sf2->add(idPrstInst, false);
            val.dwValue = idInst.indexElt;
            _sf2->set(idPrstInst, champ_instrument, val, false);

            // Remplissage de l'instrument et création des samples
            _listeEnsembles[i].decode(_sf2, idInst, QFileInfo(fileName).path());

            // Détermination keyRange du preset
            int nbInstSmpl = _sf2->count(idInstSmpl);
            int keyMin = 127;
            int keyMax = 0;
            for (int j = 0; j < nbInstSmpl; j++)
            {
                idInstSmpl.indexElt2 = j;
                rangesType range = _sf2->get(idInstSmpl, champ_keyRange).rValue;
                keyMin = qMin(keyMin, (int)range.byLo);
                keyMax = qMax(keyMax, (int)range.byHi);
            }
            if (keyMin > keyMax)
            {
                keyMin = 0;
                keyMax = 127;
            }
            val.rValue.byLo = keyMin;
            val.rValue.byHi = keyMax;
            _sf2->set(idPrstInst, champ_keyRange, val, false);
        }
    }
}

QString ImportSfz::getNomInstrument(QString filePath, int &numBank, int &numPreset)
{
    QFileInfo fileInfo(filePath);
    QString nomFichier = fileInfo.completeBaseName();
    QString nomDir = fileInfo.dir().dirName();

    // Numéro de preset
    QRegExp regExp("^\\d\\d\\d.*");
    if (regExp.exactMatch(nomFichier))
    {
        numPreset = nomFichier.left(3).toInt();
        if (numPreset < 0 || numPreset > 127)
            numPreset = 0;
        nomFichier = nomFichier.right(nomFichier.length() - 3);
        if (!nomFichier.isEmpty())
        {
            QString ch = nomFichier.left(1);
            if (ch.compare("-") == 0 || ch.compare("_") == 0 || ch.compare(".") == 0 || ch.compare(" ") == 0)
                nomFichier = nomFichier.right(nomFichier.length() - 1);
        }
    }

    // Numéro de banque
    if (regExp.exactMatch(nomDir))
    {
        numBank = nomDir.left(3).toInt();
        if (numBank < 0 || numBank > 127)
            numBank = 0;
    }

    // Nom de l'instrument
    if (nomFichier.isEmpty())
        nomFichier = QObject::trUtf8("sans nom");
    return nomFichier.left(20);
}

void ImportSfz::changeBloc(QString bloc)
{
    if (bloc == "group")
    {
        _currentBloc = BLOC_GROUP;
        _listeEnsembles << EnsembleGroupes();
    }
    else if (bloc == "region")
    {
        _currentBloc = BLOC_REGION;
        _listeEnsembles.last().newGroup();
    }
    else
        _currentBloc = BLOC_UNKNOWN;
}

void ImportSfz::addOpcode(QString opcode, QString value)
{
    if (_currentBloc == BLOC_GROUP || _currentBloc == BLOC_REGION)
        _listeEnsembles.last().addParam(opcode, value);
}


void EnsembleGroupes::moveOpcodeInSamples(Parametre::OpCode opcode)
{
    if (_paramGlobaux.isDefined(opcode))
    {
        int value = _paramGlobaux.getIntValue(opcode);
        for (int i = 0; i < _listeDivisions.size(); i++)
        {
            if (!_listeDivisions.at(i).isDefined(opcode))
                _listeDivisions[i] << Parametre(opcode, value);
        }
    }
}

void EnsembleGroupes::checkFilter()
{
    _paramGlobaux.checkFilter();
    for (int i = 0; i < _listeDivisions.size(); i++)
        _listeDivisions[i].checkFilter();
}

void EnsembleGroupes::decode(Pile_sf2 * sf2, EltID idInst, QString pathSfz)
{
    // Remplissage des paramètres globaux
    _paramGlobaux.decode(sf2, idInst);

    // Lien avec samples
    EltID idInstSmpl = idInst;
    idInstSmpl.typeElement = elementInstSmpl;
    EltID idSmpl = idInst;
    idSmpl.typeElement = elementSmpl;
    Valeur val;
    for (int i = 0; i < _listeDivisions.size(); i++)
    {
        // Création des samples si besoin et récupération de leur index
        QList<int> listeIndexSmpl = _listeDivisions.at(i).getSampleIndex(sf2, idInst, pathSfz);

        // Tranformation des offsets si présents
        if (!listeIndexSmpl.isEmpty())
        {
            idSmpl.indexElt = listeIndexSmpl.first();
            _listeDivisions[i].adaptOffsets(sf2->get(idSmpl, champ_dwStartLoop).dwValue,
                                            sf2->get(idSmpl, champ_dwEndLoop).dwValue,
                                            sf2->get(idSmpl, champ_dwLength).dwValue);
        }

        if (_listeDivisions.at(i).isDefined(Parametre::op_pan) && listeIndexSmpl.size() == 2)
        {
            double pan = _listeDivisions.at(i).getDoubleValue(Parametre::op_pan);
            if (qAbs(pan) >= 99.9)
            {
                // Passage en mono
                if (pan < 0)
                    listeIndexSmpl.removeAt(1);
                else
                    listeIndexSmpl.removeAt(0);
            }
        }

        if (listeIndexSmpl.size() == 1) // Mono sample
        {
            // Création InstSmpl
            idInstSmpl.indexElt2 = sf2->add(idInstSmpl, false);

            // Lien avec le sample
            val.wValue = listeIndexSmpl.first();
            sf2->set(idInstSmpl, champ_sampleID, val, false);

            // Remplissage paramètres de la division
            _listeDivisions.at(i).decode(sf2, idInstSmpl);

            if (_listeDivisions.at(i).isDefined(Parametre::op_pan))
            {
                val.shValue = 5 * _listeDivisions.at(i).getDoubleValue(Parametre::op_pan);
                sf2->set(idInstSmpl, champ_pan, val, false);
            }
        }
        else if (listeIndexSmpl.size() == 2) // Sample stereo
        {
            // Gestion width / position
            double width = 500;
            if (_listeDivisions.at(i).isDefined(Parametre::op_width))
                width = 5. * _listeDivisions.at(i).getDoubleValue(Parametre::op_width);
            double position = 0;
            if (_listeDivisions.at(i).isDefined(Parametre::op_position))
                position = _listeDivisions.at(i).getDoubleValue(Parametre::op_position) / 100.;
            if (position < 0)
                position = -qAbs(position * (500 - qAbs(width)));
            else
                position = qAbs(position * (500 - qAbs(width)));

            // Gestion pan
            double attenuation = 0;
            int panDefined = -1;
            if (_listeDivisions.at(i).isDefined(Parametre::op_pan))
            {
                double pan = _listeDivisions.at(i).getDoubleValue(Parametre::op_pan);
                attenuation = -GroupeParametres::percentToDB(100 - qAbs(pan));
                if (pan < 0)
                    panDefined = 1;
                else if (pan > 0)
                    panDefined = 0;
            }

            for (int j = 0; j < listeIndexSmpl.size(); j++)
            {
                // Création InstSmpl
                idInstSmpl.indexElt2 = sf2->add(idInstSmpl, false);

                // Lien avec le sample
                val.wValue = listeIndexSmpl.at(j);
                sf2->set(idInstSmpl, champ_sampleID, val, false);

                // Pan
                if (panDefined == j)
                {
                    val.shValue = 10 * attenuation;
                    sf2->set(idInstSmpl, champ_initialAttenuation, val, false);
                }

                // Remplissage paramètres de la division
                _listeDivisions.at(i).decode(sf2, idInstSmpl);

                // Width, position
                if (j == 0)
                    val.shValue = -width + position;
                else
                    val.shValue = width + position;
                sf2->set(idInstSmpl, champ_pan, val, false);
            }
        }
    }
}


QList<int> GroupeParametres::getSampleIndex(Pile_sf2 *sf2, EltID idElt, QString pathSfz) const
{
    QList<int> sampleIndex;

    int indexOpSample = -1;
    for (int i = 0; i < _listeParam.size(); i++)
        if (_listeParam.at(i).getOpCode() == Parametre::op_sample)
            indexOpSample = i;

    if (indexOpSample == -1)
        return sampleIndex;

    // Reconstitution adresse du fichier
    QString filePath =  _listeParam.at(indexOpSample).getStringValue();
    QString fileName = pathSfz + "/" + filePath.replace("\\", "/");
    if (!QFile(fileName).exists())
    {
        QStringList list = getFullPath(pathSfz, filePath.split("/", QString::SkipEmptyParts));
        if (!list.isEmpty())
            fileName = list.first();
        else
            return sampleIndex;
    }

    // Sample déjà chargé ?
    idElt.typeElement = elementSmpl;
    int nbSmpl = sf2->count(idElt);
    QStringList names;
    for (int i = 0; i < nbSmpl; i++)
    {
        idElt.indexElt = i;
        if (sf2->getQstr(idElt, champ_filename) == fileName)
            sampleIndex << i;
        names << sf2->getQstr(idElt, champ_name);
    }
    if (!sampleIndex.isEmpty())
        return sampleIndex;

    // Récupération des informations d'un sample
    Sound son(fileName);
    int nChannels = son.get(champ_wChannels);
    QString nom = QFileInfo(fileName).completeBaseName();

    // Création d'un nouveau sample
    Valeur val;
    for (int i = 0; i < nChannels; i++)
    {
        idElt.indexElt = sf2->add(idElt, false);
        sampleIndex << idElt.indexElt;
        if (nChannels == 2)
        {
            if (i == 0)
            {
                // Gauche
                sf2->set(idElt, champ_name, nom.left(19).append("L"), false);
                val.wValue = idElt.indexElt + 1;
                sf2->set(idElt, champ_wSampleLink, val, false);
                val.sfLinkValue = leftSample;
                sf2->set(idElt, champ_sfSampleType, val, false);
            }
            else
            {
                // Droite
                sf2->set(idElt, champ_name, nom.left(19).append("R"), false);
                val.wValue = idElt.indexElt - 1;
                sf2->set(idElt, champ_wSampleLink, val, false);
                val.sfLinkValue = rightSample;
                sf2->set(idElt, champ_sfSampleType, val, false);
            }
        }
        else
        {
            sf2->set(idElt, champ_name, QString(nom.left(20)), false);
            val.wValue = 0;
            sf2->set(idElt, champ_wSampleLink, val, false);
            val.sfLinkValue = monoSample;
            sf2->set(idElt, champ_sfSampleType, val, false);
        }
        sf2->set(idElt, champ_filename, fileName, false);
        val.dwValue = son.get(champ_dwStart16);
        sf2->set(idElt, champ_dwStart16, val, false);
        val.dwValue = son.get(champ_dwStart24);
        sf2->set(idElt, champ_dwStart24, val, false);
        val.wValue = i;
        sf2->set(idElt, champ_wChannel, val, false);
        val.dwValue = son.get(champ_dwLength);
        sf2->set(idElt, champ_dwLength, val, false);
        val.dwValue = son.get(champ_dwSampleRate);
        sf2->set(idElt, champ_dwSampleRate, val, false);
        val.dwValue = son.get(champ_dwStartLoop);
        sf2->set(idElt, champ_dwStartLoop, val, false);
        val.dwValue = son.get(champ_dwEndLoop);
        sf2->set(idElt, champ_dwEndLoop, val, false);
        val.bValue = (BYTE)son.get(champ_byOriginalPitch);
        sf2->set(idElt, champ_byOriginalPitch, val, false);
        val.cValue = (char)son.get(champ_chPitchCorrection);
        sf2->set(idElt, champ_chPitchCorrection, val, false);

        // Chargement dans la ram
        if (Config::getInstance()->getRam())
        {
            val.wValue = 1;
            sf2->set(idElt, champ_ram, val, false);
        }
    }

    return sampleIndex;
}

void GroupeParametres::adaptOffsets(int startLoop, int endLoop, int length)
{
    bool loopStartDefined = false;
    bool loopEndDefined = false;

    for (int i = 0; i < _listeParam.size(); i++)
    {
        if (_listeParam.at(i).getOpCode() == Parametre::op_loop_start)
        {
            loopStartDefined = true;
            _listeParam[i].setIntValue(_listeParam.at(i).getIntValue() - startLoop);
        }
        else if (_listeParam.at(i).getOpCode() == Parametre::op_loop_end)
        {
            loopEndDefined = true;
            _listeParam[i].setIntValue(_listeParam.at(i).getIntValue() - endLoop);
        }
        else if (_listeParam.at(i).getOpCode() == Parametre::op_end)
            _listeParam[i].setIntValue(_listeParam.at(i).getIntValue() - length);
    }
    if (loopStartDefined && loopEndDefined && !isDefined(Parametre::op_loop_mode))
        _listeParam << Parametre("loop_mode", "loop_continuous");
}

void GroupeParametres::checkFilter()
{
    bool removeFilter = false;
    if (isDefined(Parametre::op_filterType))
    {
        QString type = getStrValue(Parametre::op_filterType);
        if (type != "lpf_2p" && type != "lpf_1p") // 1 pôle : acceptable
            removeFilter = true;
    }

    if (removeFilter)
    {
        for (int i = _listeParam.size() - 1; i >= 0; i--)
        {
            if (_listeParam.at(i).getOpCode() == Parametre::op_filterFreq ||
                    _listeParam.at(i).getOpCode() == Parametre::op_filterType ||
                    _listeParam.at(i).getOpCode() == Parametre::op_filterQ)
                _listeParam.removeAt(i);
        }
    }
}

QStringList GroupeParametres::getFullPath(QString base, QStringList directories)
{
    QStringList listRet;
    if (directories.isEmpty())
        listRet << base;
    else
    {
        QDir dir(base);
        dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
        QStringList childs = dir.entryList();
        QString nextDirectory = directories.takeFirst();
        for (int i = 0; i < childs.size(); i++)
            if (childs.at(i).compare(nextDirectory, Qt::CaseInsensitive) == 0)
                listRet << getFullPath(base + "/" + childs.at(i), directories);
    }
    return listRet;
}

void GroupeParametres::decode(Pile_sf2 * sf2, EltID idElt) const
{
    Valeur val;
    double dTmp;
    for (int i = 0; i < _listeParam.size(); i++)
    {
        switch (_listeParam.at(i).getOpCode())
        {
        case Parametre::op_key:
            val.rValue.byLo = _listeParam.at(i).getIntValue();
            val.rValue.byHi = val.rValue.byLo;
            sf2->set(idElt, champ_keyRange, val, false);
            break;
        case Parametre::op_keyMin:
            if (sf2->isSet(idElt, champ_keyRange))
                val = sf2->get(idElt, champ_keyRange);
            else
                val.rValue.byHi = 127;
            val.rValue.byLo = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_keyRange, val, false);
            break;
        case Parametre::op_keyMax:
            if (sf2->isSet(idElt, champ_keyRange))
                val = sf2->get(idElt, champ_keyRange);
            else
                val.rValue.byLo = 0;
            val.rValue.byHi = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_keyRange, val, false);
            break;
        case Parametre::op_velMin:
            if (sf2->isSet(idElt, champ_velRange))
                val = sf2->get(idElt, champ_velRange);
            else
                val.rValue.byHi = 127;
            val.rValue.byLo = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_velRange, val, false);
            break;
        case Parametre::op_velMax:
            if (sf2->isSet(idElt, champ_velRange))
                val = sf2->get(idElt, champ_velRange);
            else
                val.rValue.byLo = 0;
            val.rValue.byHi = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_velRange, val, false);
            break;
        case Parametre::op_rootKey:
            val.wValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_overridingRootKey, val, false);
            break;
        case Parametre::op_exclusiveClass:
            val.wValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_exclusiveClass, val, false);
            break;
        case Parametre::op_tuningFine:
            val.wValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_fineTune, val, false);
            break;
        case Parametre::op_tuningCoarse:
            val.wValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_coarseTune, val, false);
            break;
        case Parametre::op_delay:
            dTmp = _listeParam.at(i).getDoubleValue();
            addSeconds(dTmp, champ_delayModEnv, sf2, idElt);
            addSeconds(dTmp, champ_delayModLFO, sf2, idElt);
            addSeconds(dTmp, champ_delayVibLFO, sf2, idElt);
            addSeconds(dTmp, champ_delayVolEnv, sf2, idElt);
            break;
        case Parametre::op_offset:
            if (idElt.typeElement == elementInstSmpl)
            {
                val.shValue = _listeParam.at(i).getIntValue() % 32768;
                sf2->set(idElt, champ_startAddrsOffset, val, false);
                val.shValue = _listeParam.at(i).getIntValue() / 32768;
                sf2->set(idElt, champ_startAddrsCoarseOffset, val, false);
            }
            break;
        case Parametre::op_end:
            if (idElt.typeElement == elementInstSmpl)
            {
                val.shValue = _listeParam.at(i).getIntValue() % 32768;
                sf2->set(idElt, champ_endAddrsOffset, val, false);
                val.shValue = _listeParam.at(i).getIntValue() / 32768;
                sf2->set(idElt, champ_endAddrsCoarseOffset, val, false);
            }
            break;
        case Parametre::op_loop_start:
            if (idElt.typeElement == elementInstSmpl)
            {
                val.shValue = _listeParam.at(i).getIntValue() % 32768;
                sf2->set(idElt, champ_startloopAddrsOffset, val, false);
                val.shValue = _listeParam.at(i).getIntValue() / 32768;
                sf2->set(idElt, champ_startloopAddrsCoarseOffset, val, false);
            }
            break;
        case Parametre::op_loop_end:
            if (idElt.typeElement == elementInstSmpl)
            {
                val.shValue = _listeParam.at(i).getIntValue() % 32768;
                sf2->set(idElt, champ_endloopAddrsOffset, val, false);
                val.shValue = _listeParam.at(i).getIntValue() / 32768;
                sf2->set(idElt, champ_endloopAddrsCoarseOffset, val, false);
            }
            break;
        case Parametre::op_loop_mode:
            if (_listeParam.at(i).getStringValue() == "no_loop")
                val.wValue = 0;
            else if (_listeParam.at(i).getStringValue() == "one_shot")
            {
                val.wValue = 0;
                addSeconds(100, champ_releaseVolEnv, sf2, idElt);
                addSeconds(100, champ_releaseModEnv, sf2, idElt);
            }
            else if (_listeParam.at(i).getStringValue() == "loop_continuous")
                val.wValue = 1;
            else if (_listeParam.at(i).getStringValue() == "loop_sustain")
                val.wValue = 3;
            sf2->set(idElt, champ_sampleModes, val, false);
            break;
        case Parametre::op_volume:
            dTmp = qMax(0., -_listeParam.at(i).getDoubleValue());
            if (sf2->isSet(idElt, champ_initialAttenuation))
                dTmp += (double)sf2->get(idElt, champ_initialAttenuation).shValue / 10.;
            val.shValue = 10. * dTmp + .5;
            sf2->set(idElt, champ_initialAttenuation, val, false);
            break;
        case Parametre::op_tuningScale:
            val.shValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_scaleTuning, val, false);
            break;
        case Parametre::op_ampeg_delay:
            addSeconds(_listeParam.at(i).getDoubleValue(), champ_delayVolEnv, sf2, idElt);
            break;
        case Parametre::op_ampeg_attack:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_attackVolEnv, val, false);
            break;
        case Parametre::op_ampeg_hold:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_holdVolEnv, val, false);
            break;
        case Parametre::op_ampeg_decay:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_decayVolEnv, val, false);
            break;
        case Parametre::op_ampeg_sustain:
            dTmp = _listeParam.at(i).getDoubleValue();
            if (dTmp >= 0.1)
                val.shValue = -10. * percentToDB(dTmp) + .5;
            else
                val.shValue = 1440;
            sf2->set(idElt, champ_sustainVolEnv, val, false);
            break;
        case Parametre::op_ampeg_release:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_releaseVolEnv, val, false);
            break;
        case Parametre::op_noteToVolEnvHold:
            val.shValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_keynumToVolEnvHold, val, false);
            break;
        case Parametre::op_noteToVolEnvDecay:
            val.shValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_keynumToVolEnvDecay, val, false);
            break;
        case Parametre::op_reverb:
            val.wValue = 10. * _listeParam.at(i).getDoubleValue() + .5;
            sf2->set(idElt, champ_reverbEffectsSend, val, false);
            break;
        case Parametre::op_chorus:
            val.wValue = 10. * _listeParam.at(i).getDoubleValue() + .5;
            sf2->set(idElt, champ_chorusEffectsSend, val, false);
            break;
        case Parametre::op_filterFreq:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue() / 8.176) + .5;
            sf2->set(idElt, champ_initialFilterFc, val, false);
            break;
        case Parametre::op_filterQ:
            val.shValue = 10. * _listeParam.at(i).getDoubleValue() + .5;
            sf2->set(idElt, champ_initialFilterQ, val, false);
            break;
        case Parametre::op_vibLFOdelay:
            if (isDefined(Parametre::op_vibLFOtoTon))
                addSeconds(_listeParam.at(i).getDoubleValue(), champ_delayVibLFO, sf2, idElt);
            break;
        case Parametre::op_vibLFOfreq:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue() / 8.176);
            sf2->set(idElt, champ_freqVibLFO, val, false);
            break;
        case Parametre::op_vibLFOtoTon:
            val.shValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_vibLfoToPitch, val, false);
            break;
        case Parametre::op_pitcheg_delay:
            if (isDefined(Parametre::op_modEnvToTon))
                addSeconds(_listeParam.at(i).getDoubleValue(), champ_delayModEnv, sf2, idElt);
            break;
        case Parametre::op_pitcheg_attack:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_attackModEnv, val, false);
            break;
        case Parametre::op_pitcheg_hold:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_holdModEnv, val, false);
            break;
        case Parametre::op_pitcheg_decay:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_decayModEnv, val, false);
            break;
        case Parametre::op_pitcheg_sustain:
            dTmp = _listeParam.at(i).getDoubleValue();
            if (dTmp >= 0.1)
                val.shValue = -10. * percentToDB(dTmp) + .5;
            else
                val.shValue = 1440;
            sf2->set(idElt, champ_sustainModEnv, val, false);
            break;
        case Parametre::op_pitcheg_release:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue()) + .5;
            sf2->set(idElt, champ_releaseModEnv, val, false);
            break;
        case Parametre::op_modEnvToTon:
            val.shValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_modEnvToPitch, val, false);
            break;
        case Parametre::op_noteToModEnvHold:
            val.shValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_keynumToModEnvHold, val, false);
            break;
        case Parametre::op_noteToModEnvDecay:
            val.shValue = _listeParam.at(i).getIntValue();
            sf2->set(idElt, champ_keynumToModEnvDecay, val, false);
            break;
        case Parametre::op_modLFOdelay:
            addSeconds(_listeParam.at(i).getDoubleValue(), champ_delayModLFO, sf2, idElt);
            break;
        case Parametre::op_modLFOfreq:
            val.shValue = log2m1200(_listeParam.at(i).getDoubleValue() / 8.176);
            sf2->set(idElt, champ_freqModLFO, val, false);
            break;
        case Parametre::op_modLFOtoVolume:
            val.shValue = 10. * _listeParam.at(i).getDoubleValue();
            sf2->set(idElt, champ_modLfoToVolume, val, false);
            break;
        default:
            break;
        }
    }

    // Vélocité fixe
    if (isDefined(Parametre::op_amp_velcurve_1) && isDefined(Parametre::op_amp_velcurve_127))
    {
        int velMin = 127. * getDoubleValue(Parametre::op_amp_velcurve_1) + .5;
        int velMax = 127. * getDoubleValue(Parametre::op_amp_velcurve_127) + .5;
        if (velMin == velMax)
        {
            val.wValue = velMin;
            sf2->set(idElt, champ_velocity, val, false);
        }
    }

    // Enveloppe pour le filtre
    if (isDefined(Parametre::op_modEnvToFilter))
    {
        if (isDefined(Parametre::op_modEnvToTon))
        {
            // Mêmes paramètres ?
            if (getDoubleValue(Parametre::op_fileg_delay) == getDoubleValue(Parametre::op_pitcheg_delay) &&
                getDoubleValue(Parametre::op_fileg_attack) == getDoubleValue(Parametre::op_pitcheg_attack) &&
                getDoubleValue(Parametre::op_fileg_hold) == getDoubleValue(Parametre::op_pitcheg_hold) &&
                getDoubleValue(Parametre::op_fileg_decay) == getDoubleValue(Parametre::op_pitcheg_decay) &&
                getDoubleValue(Parametre::op_fileg_sustain) == getDoubleValue(Parametre::op_pitcheg_sustain) &&
                getDoubleValue(Parametre::op_fileg_release) == getDoubleValue(Parametre::op_pitcheg_release) &&
                getIntValue(Parametre::op_fileg_holdcc133) == getIntValue(Parametre::op_noteToModEnvHold) &&
                getIntValue(Parametre::op_fileg_decaycc133) == getIntValue(Parametre::op_noteToModEnvDecay))
            {
                val.shValue = getIntValue(Parametre::op_modEnvToFilter);
                sf2->set(idElt, champ_modEnvToFilterFc, val, false);
            }
        }
        else
        {
            if (isDefined(Parametre::op_fileg_delay))
                addSeconds(getDoubleValue(Parametre::op_fileg_delay), champ_delayModEnv, sf2, idElt);
            if (isDefined(Parametre::op_fileg_attack))
            {
                val.shValue = log2m1200(getDoubleValue(Parametre::op_fileg_attack)) + .5;
                sf2->set(idElt, champ_attackModEnv, val, false);
            }
            if (isDefined(Parametre::op_fileg_hold))
            {
                val.shValue = log2m1200(getDoubleValue(Parametre::op_fileg_hold)) + .5;
                sf2->set(idElt, champ_holdModEnv, val, false);
            }
            if (isDefined(Parametre::op_fileg_decay))
            {
                val.shValue = log2m1200(getDoubleValue(Parametre::op_fileg_decay)) + .5;
                sf2->set(idElt, champ_decayModEnv, val, false);
            }
            if (isDefined(Parametre::op_fileg_sustain))
            {
                dTmp = getDoubleValue(Parametre::op_fileg_sustain);
                if (dTmp >= 0.1)
                    val.shValue = -10. * percentToDB(dTmp) + .5;
                else
                    val.shValue = 1440;
                sf2->set(idElt, champ_sustainModEnv, val, false);
            }
            if (isDefined(Parametre::op_fileg_release))
            {
                val.shValue = log2m1200(getDoubleValue(Parametre::op_fileg_release)) + .5;
                sf2->set(idElt, champ_releaseModEnv, val, false);
            }
            if (isDefined(Parametre::op_fileg_holdcc133))
            {
                val.shValue = getIntValue(Parametre::op_fileg_holdcc133);
                sf2->set(idElt, champ_keynumToModEnvHold, val, false);
            }
            if (isDefined(Parametre::op_fileg_decaycc133))
            {
                val.shValue = getIntValue(Parametre::op_fileg_decaycc133);
                sf2->set(idElt, champ_keynumToModEnvDecay, val, false);
            }

            val.shValue = getIntValue(Parametre::op_modEnvToFilter);
            sf2->set(idElt, champ_modEnvToFilterFc, val, false);
        }
    }

    // LFO filtre
    if (isDefined(Parametre::op_modLFOtoFilter))
    {
        if (isDefined(Parametre::op_modLFOtoVolume))
        {
            // Mêmes paramètres ?
            if (getDoubleValue(Parametre::op_filLFOdelay) == getDoubleValue(Parametre::op_modLFOdelay) &&
                getDoubleValue(Parametre::op_filLFOfreq) == getDoubleValue(Parametre::op_modLFOfreq))
            {
                val.shValue = getIntValue(Parametre::op_modLFOtoFilter);
                sf2->set(idElt, champ_modLfoToFilterFc, val, false);
            }
        }
        else
        {
            if (isDefined(Parametre::op_filLFOdelay))
                addSeconds(getDoubleValue(Parametre::op_filLFOdelay), champ_delayModLFO, sf2, idElt);
            if (isDefined(Parametre::op_filLFOfreq))
            {
                val.shValue = log2m1200(getDoubleValue(Parametre::op_filLFOfreq) / 8.176);
                sf2->set(idElt, champ_freqModLFO, val, false);
            }
            val.shValue = getIntValue(Parametre::op_modLFOtoFilter);
            sf2->set(idElt, champ_modLfoToFilterFc, val, false);
        }
    }
}

void GroupeParametres::addSeconds(double value, Champ champ, Pile_sf2 * sf2, EltID id)
{
    double dTmp;
    Valeur val;
    if (sf2->isSet(id, champ))
        dTmp = d1200e2(sf2->get(id, champ).shValue);
    else
        dTmp = 0;
    val.shValue = log2m1200(dTmp + value) + 0.5;
    sf2->set(id, champ, val, false);
}


Parametre::Parametre(QString opcode, QString valeur) :
    _opcode(op_unknown),
    _intValue(0),
    _dblValue(0.)
{
    QString valeurLow = valeur.toLower();
    if (opcode == "sample")
    {
        _opcode = op_sample;
        _strValue = valeur;
    }
    else if (opcode == "key")
    {
        _opcode = op_key;
        _intValue = getNumNote(valeurLow);
    }
    else if (opcode == "lokey")
    {
        _opcode = op_keyMin;
        _intValue = getNumNote(valeurLow);
    }
    else if (opcode == "hikey")
    {
        _opcode = op_keyMax;
        _intValue = getNumNote(valeurLow);
    }
    else if (opcode == "lovel")
    {
        _opcode = op_velMin;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "hivel")
    {
        _opcode = op_velMax;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "pitch_keycenter")
    {
        _opcode = op_rootKey;
        _intValue = getNumNote(valeurLow);
    }
    else if (opcode == "group")
    {
        _opcode = op_exclusiveClass;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "tune")
    {
        _opcode = op_tuningFine;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "transpose")
    {
        _opcode = op_tuningCoarse;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "delay")
    {
        _opcode = op_delay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "offset")
    {
        _opcode = op_offset;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "end")
    {
        _opcode = op_end;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "loop_start")
    {
        _opcode = op_loop_start;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "loop_end")
    {
        _opcode = op_loop_end;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "loop_mode")
    {
        _opcode = op_loop_mode;
        _strValue = valeurLow;
    }
    else if (opcode == "pan")
    {
        _opcode = op_pan;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "width")
    {
        _opcode = op_width;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "position")
    {
        _opcode = op_position;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "volume")
    {
        _opcode = op_volume;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitch_keytrack")
    {
        _opcode = op_tuningScale;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "ampeg_delay")
    {
        _opcode = op_ampeg_delay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "ampeg_attack")
    {
        _opcode = op_ampeg_attack;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "ampeg_hold")
    {
        _opcode = op_ampeg_hold;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "ampeg_decay")
    {
        _opcode = op_ampeg_decay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "ampeg_sustain")
    {
        _opcode = op_ampeg_sustain;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "ampeg_release")
    {
        _opcode = op_ampeg_release;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "ampeg_holdcc133")
    {
        _opcode = op_noteToVolEnvHold;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "ampeg_decaycc133")
    {
        _opcode = op_noteToVolEnvDecay;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "effect1")
    {
        _opcode = op_reverb;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "effect2")
    {
        _opcode = op_chorus;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fil_type")
    {
        _opcode = op_filterType;
        _strValue = valeurLow;
    }
    else if (opcode == "cutoff")
    {
        _opcode = op_filterFreq;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "resonance")
    {
        _opcode = op_filterQ;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitchlfo_delay")
    {
        _opcode = op_vibLFOdelay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitchlfo_freq")
    {
        _opcode = op_vibLFOfreq;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitchlfo_depth")
    {
        _opcode = op_vibLFOtoTon;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "pitcheg_delay")
    {
        _opcode = op_pitcheg_delay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitcheg_attack")
    {
        _opcode = op_pitcheg_attack;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitcheg_hold")
    {
        _opcode = op_pitcheg_hold;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitcheg_decay")
    {
        _opcode = op_pitcheg_decay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitcheg_sustain")
    {
        _opcode = op_pitcheg_sustain;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitcheg_release")
    {
        _opcode = op_pitcheg_release;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "pitcheg_depth")
    {
        _opcode = op_modEnvToTon;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "pitcheg_holdcc133")
    {
        _opcode = op_noteToModEnvHold;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "pitcheg_decaycc133")
    {
        _opcode = op_noteToModEnvDecay;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "amplfo_delay")
    {
        _opcode = op_modLFOdelay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "amplfo_freq")
    {
        _opcode = op_modLFOfreq;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "amplfo_depth")
    {
        _opcode = op_modLFOtoVolume;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "amp_velcurve_1")
    {
        _opcode = op_amp_velcurve_1;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "amp_velcurve_127")
    {
        _opcode = op_amp_velcurve_127;
        _dblValue = valeurLow.toDouble();
    }


    else if (opcode == "fileg_delay")
    {
        _opcode = op_fileg_delay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fileg_attack")
    {
        _opcode = op_fileg_attack;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fileg_hold")
    {
        _opcode = op_fileg_hold;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fileg_decay")
    {
        _opcode = op_fileg_decay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fileg_sustain")
    {
        _opcode = op_fileg_sustain;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fileg_release")
    {
        _opcode = op_fileg_release;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fileg_depth")
    {
        _opcode = op_modEnvToFilter;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "fileg_holdcc133")
    {
        _opcode = op_fileg_holdcc133;
        _intValue = valeurLow.toInt();
    }
    else if (opcode == "fileg_decaycc133")
    {
        _opcode = op_fileg_decaycc133;
        _intValue = valeurLow.toInt();
    }


    else if (opcode == "fillfo_delay")
    {
        _opcode = op_filLFOdelay;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fillfo_freq")
    {
        _opcode = op_filLFOfreq;
        _dblValue = valeurLow.toDouble();
    }
    else if (opcode == "fillfo_depth")
    {
        _opcode = op_modLFOtoFilter;
        _intValue = valeurLow.toInt();
    }


    else
        qDebug() << "non pris en charge: " + opcode + " (" + valeur + ")";
}

int Parametre::getNumNote(QString noteStr)
{
    int note = noteStr.toInt();
    if (note == 0 && noteStr != "0" && noteStr.size() >= 2)
    {
        switch (noteStr.at(0).toAscii())
        {
        case 'c': note = 60; break;
        case 'd': note = 62; break;
        case 'e': note = 64; break;
        case 'f': note = 65; break;
        case 'g': note = 67; break;
        case 'a': note = 69; break;
        case 'b': note = 71; break;
        default : return -1; break;
        }
        noteStr = noteStr.right(noteStr.size() - 1);
        if (noteStr.at(0).toAscii() == '#')
        {
            note ++;
            noteStr = noteStr.right(noteStr.size() - 1);
        }
        else if (noteStr.at(0).toAscii() == 'b')
        {
            note --;
            noteStr = noteStr.right(noteStr.size() - 1);
        }

        int octave = noteStr.toInt();
        if ((octave == 0 && noteStr != "0") || noteStr.isEmpty())
            return -1;
        else
            note += (octave - 4) * 12;
    }
    return note;
}