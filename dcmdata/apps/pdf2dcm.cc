/*
 *
 *  Copyright (C) 2005-2009, OFFIS
 *
 *  This software and supporting documentation were developed by
 *
 *    Kuratorium OFFIS e.V.
 *    Healthcare Information and Communication Systems
 *    Escherweg 2
 *    D-26121 Oldenburg, Germany
 *
 *  THIS SOFTWARE IS MADE AVAILABLE,  AS IS,  AND OFFIS MAKES NO  WARRANTY
 *  REGARDING  THE  SOFTWARE,  ITS  PERFORMANCE,  ITS  MERCHANTABILITY  OR
 *  FITNESS FOR ANY PARTICULAR USE, FREEDOM FROM ANY COMPUTER DISEASES  OR
 *  ITS CONFORMITY TO ANY SPECIFICATION. THE ENTIRE RISK AS TO QUALITY AND
 *  PERFORMANCE OF THE SOFTWARE IS WITH THE USER.
 *
 *  Module:  dcmdata
 *
 *  Author:  Marco Eichelberg
 *
 *  Purpose: Convert PDF file to DICOM format
 *
 *  Last Update:      $Author: joergr $
 *  Update Date:      $Date: 2009-04-21 14:02:49 $
 *  CVS/RCS Revision: $Revision: 1.10 $
 *  Status:           $State: Exp $
 *
 *  CVS/RCS Log at end of file
 *
 */

#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#define INCLUDE_CSTDLIB
#define INCLUDE_CSTDIO
#define INCLUDE_CSTRING
#include "dcmtk/ofstd/ofstdinc.h"

#ifdef HAVE_GUSI_H
#include <GUSI.h>
#endif

BEGIN_EXTERN_C
#ifdef HAVE_FCNTL_H
#include <fcntl.h>       /* for O_RDONLY */
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>   /* required for sys/stat.h */
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>    /* for stat, fstat */
#endif
END_EXTERN_C

#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/dcmdata/dcuid.h"       /* for dcmtk version name */
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/ofstd/ofdatime.h"
#include "dcmtk/dcmdata/dccodec.h"
#include "dcmtk/dcmdata/dcdebug.h"

#ifdef WITH_ZLIB
#include <zlib.h>        /* for zlibVersion() */
#endif

#define OFFIS_CONSOLE_APPLICATION "pdf2dcm"

static char rcsid[] = "$dcmtk: " OFFIS_CONSOLE_APPLICATION " v"
  OFFIS_DCMTK_VERSION " " OFFIS_DCMTK_RELEASEDATE " $";

OFCondition createHeader(
  DcmItem *dataset,
  const char *opt_patientsName,
  const char *opt_patientID,
  const char *opt_patientsBirthdate,
  const char *opt_patientsSex,
  OFBool opt_burnedInAnnotation,
  const char *opt_studyUID,
  const char *opt_seriesUID,
  const char *opt_documentTitle,
  const char *opt_conceptCSD,
  const char *opt_conceptCV,
  const char *opt_conceptCM,
  Sint32 opt_instanceNumber)
{
    OFCondition result = EC_Normal;
    char buf[80];

    // insert empty type 2 attributes
    if (result.good()) result = dataset->insertEmptyElement(DCM_StudyDate);
    if (result.good()) result = dataset->insertEmptyElement(DCM_StudyTime);
    if (result.good()) result = dataset->insertEmptyElement(DCM_AccessionNumber);
    if (result.good()) result = dataset->insertEmptyElement(DCM_Manufacturer);
    if (result.good()) result = dataset->insertEmptyElement(DCM_ReferringPhysiciansName);
    if (result.good()) result = dataset->insertEmptyElement(DCM_StudyID);
    if (result.good()) result = dataset->insertEmptyElement(DCM_ContentDate);
    if (result.good()) result = dataset->insertEmptyElement(DCM_ContentTime);
    if (result.good()) result = dataset->insertEmptyElement(DCM_AcquisitionDateTime);

    if (result.good() && opt_conceptCSD && opt_conceptCV && opt_conceptCM)
    {
      result = DcmCodec::insertCodeSequence(dataset, DCM_ConceptNameCodeSequence, opt_conceptCSD, opt_conceptCV, opt_conceptCM);
    }
    else
    {
      result = dataset->insertEmptyElement(DCM_ConceptNameCodeSequence);
    }

    // insert const value attributes
    if (result.good()) result = dataset->putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 100");
    if (result.good()) result = dataset->putAndInsertString(DCM_SOPClassUID,          UID_EncapsulatedPDFStorage);
    if (result.good()) result = dataset->putAndInsertString(DCM_Modality,             "OT");
    if (result.good()) result = dataset->putAndInsertString(DCM_ConversionType,       "WSD");
    if (result.good()) result = dataset->putAndInsertString(DCM_MIMETypeOfEncapsulatedDocument, "application/pdf");

    // there is no way we could determine a meaningful series number, so we just use a constant.
    if (result.good()) result = dataset->putAndInsertString(DCM_SeriesNumber,         "1");

    // insert variable value attributes
    if (result.good()) result = dataset->putAndInsertString(DCM_DocumentTitle,        opt_documentTitle);
    if (result.good()) result = dataset->putAndInsertString(DCM_PatientsName,         opt_patientsName);
    if (result.good()) result = dataset->putAndInsertString(DCM_PatientID,            opt_patientID);
    if (result.good()) result = dataset->putAndInsertString(DCM_PatientsBirthDate,    opt_patientsBirthdate);
    if (result.good()) result = dataset->putAndInsertString(DCM_PatientsSex,          opt_patientsSex);
    if (result.good()) result = dataset->putAndInsertString(DCM_BurnedInAnnotation,   opt_burnedInAnnotation ? "YES" : "NO");

    sprintf(buf, "%ld", OFstatic_cast(long, opt_instanceNumber));
    if (result.good()) result = dataset->putAndInsertString(DCM_InstanceNumber,       buf);

    dcmGenerateUniqueIdentifier(buf);
    if (result.good()) result = dataset->putAndInsertString(DCM_StudyInstanceUID,     opt_studyUID);
    if (result.good()) result = dataset->putAndInsertString(DCM_SeriesInstanceUID,    opt_seriesUID);
    if (result.good()) result = dataset->putAndInsertString(DCM_SOPInstanceUID,       buf);

    // set instance creation date and time
    OFString s;
    if (result.good()) result = DcmDate::getCurrentDate(s);
    if (result.good()) result = dataset->putAndInsertOFStringArray(DCM_InstanceCreationDate, s);
    if (result.good()) result = DcmTime::getCurrentTime(s);
    if (result.good()) result = dataset->putAndInsertOFStringArray(DCM_InstanceCreationTime, s);

    return result;
}

OFCondition insertPDFFile(
  DcmItem *dataset,
  const char *filename,
  OFBool opt_verbose)
{
    size_t fileSize = 0;
    struct stat fileStat;
    char buf[100];

    if (0 == stat(filename, &fileStat)) fileSize = OFstatic_cast(size_t, fileStat.st_size);
    else
    {
      ofConsole.lockCerr() << "file " << filename << " not found" << OFendl;
      ofConsole.unlockCerr();
      return EC_IllegalCall;
    }

    if (fileSize == 0)
    {
      ofConsole.lockCerr() << "file " << filename << " is empty" << OFendl;
      ofConsole.unlockCerr();
      return EC_IllegalCall;
    }

    FILE *pdffile = fopen(filename, "rb");
    if (pdffile == NULL)
    {
      ofConsole.lockCerr() << "unable to read file " << filename << OFendl;
      ofConsole.unlockCerr();
      return EC_IllegalCall;
    }

    size_t buflen = 100;
    if (fileSize < buflen) buflen = fileSize;
    if (buflen != fread(buf, 1, buflen, pdffile))
    {
      ofConsole.lockCerr() << "read error in file " << filename << OFendl;
      ofConsole.unlockCerr();
      fclose(pdffile);
      return EC_IllegalCall;
    }

    // check magic word for PDF file
    if (0 != strncmp("%PDF-", buf, 5))
    {
      ofConsole.lockCerr() << "file " << filename << " is not a PDF file." << OFendl;
      ofConsole.unlockCerr();
      fclose(pdffile);
      return EC_IllegalCall;
    }

    // check PDF version number
    char *version = buf + 5;
    OFBool found = OFFalse;
    for (int i = 0; i < 5; ++i)
    {
      if (version[i] == 10 || version[i] == 13)
      {
      	version[i] = 0; // insert end of string
      	found = OFTrue;
      	break;
      }
    }

    if (! found)
    {
      ofConsole.lockCerr() << "file " << filename << ": unable to decode PDF version number." << OFendl;
      ofConsole.unlockCerr();
      fclose(pdffile);
      return EC_IllegalCall;
    }

    if (opt_verbose)
    {
      ofConsole.lockCout() << "file " << filename << ": PDF " << version << ", " << (fileSize+1023)/1024 << "kB" << OFendl;
      ofConsole.unlockCout();
    }

    if (0 != fseek(pdffile, 0, SEEK_SET))
    {
      ofConsole.lockCerr() << "file " << filename << ": seek error." << OFendl;
      ofConsole.unlockCerr();
      fclose(pdffile);
      return EC_IllegalCall;
    }

    OFCondition result = EC_Normal;
    DcmPolymorphOBOW *elem = new DcmPolymorphOBOW(DCM_EncapsulatedDocument);
    if (elem)
    {
      Uint32 numBytes = fileSize;
      if (numBytes & 1) ++numBytes;
      Uint8 *bytes = NULL;
      result = elem->createUint8Array(numBytes, bytes);
      if (result.good())
      {
        // blank pad byte
      	bytes[numBytes-1] = 0;

        // read PDF content
        if (fileSize != fread(bytes, 1, fileSize, pdffile))
        {
          ofConsole.lockCerr() << "read error in file " << filename << OFendl;
          ofConsole.unlockCerr();
          result = EC_IllegalCall;
        }
      }
    } else result = EC_MemoryExhausted;

    // if successful, insert element into dataset
    if (result.good()) result = dataset->insert(elem); else delete elem;

    // close file
    fclose(pdffile);

    return result;
}


void createIdentifiers(
  OFBool opt_readSeriesInfo,
  const char *opt_seriesFile,
  OFString& studyUID,
  OFString& seriesUID,
  OFString& patientsName,
  OFString& patientID,
  OFString& patientsBirthDate,
  OFString& patientsSex,
  Sint32& incrementedInstance)
{
  char buf[100];
  if (opt_seriesFile)
  {
    DcmFileFormat dfile;
    OFCondition cond = dfile.loadFile(opt_seriesFile, EXS_Unknown, EGL_noChange);
    if (cond.bad())
    {
      ofConsole.lockCerr() << "warning: " << cond.text() << ": reading file: "<< opt_seriesFile << OFendl;
      ofConsole.unlockCerr();
    }
    else
    {
      const char *c = NULL;
      DcmDataset *dset = dfile.getDataset();
      if (dset)
      {
        // read patient attributes
        c = NULL;
        if (dset->findAndGetString(DCM_PatientsName, c).good() && c) patientsName = c;
        c = NULL;
        if (dset->findAndGetString(DCM_PatientID, c).good() && c) patientID = c;
        c = NULL;
        if (dset->findAndGetString(DCM_PatientsBirthDate, c).good() && c) patientsBirthDate = c;
        c = NULL;
        if (dset->findAndGetString(DCM_PatientsSex, c).good() && c) patientsSex = c;

        // read study attributes
        c = NULL;
        if (dset->findAndGetString(DCM_StudyInstanceUID, c).good() && c) studyUID = c;

        // read series attributes
        if (opt_readSeriesInfo)
        {
          c = NULL;
          if (dset->findAndGetString(DCM_SeriesInstanceUID, c).good() && c) seriesUID = c;
          if (dset->findAndGetSint32(DCM_InstanceNumber, incrementedInstance).good()) ++incrementedInstance; else incrementedInstance = 0;
        }
      }
    }
  }
  if (studyUID.length() == 0)
  {
    dcmGenerateUniqueIdentifier(buf);
    studyUID = buf;
  }
  if (seriesUID.length() == 0)
  {
    dcmGenerateUniqueIdentifier(buf);
    seriesUID = buf;
  }
}


#define SHORTCOL 3
#define LONGCOL 19

int main(int argc, char *argv[])
{

#ifdef HAVE_GUSI_H
  GUSISetup(GUSIwithSIOUXSockets);
  GUSISetup(GUSIwithInternetSockets);
#endif

  SetDebugLevel(( 0 ));

  const char *opt_ifname = NULL;
  const char *opt_ofname = NULL;

  int opt_debugMode = 0;
  OFBool opt_verbose = OFFalse;
  E_TransferSyntax opt_oxfer = EXS_LittleEndianExplicit;
  E_GrpLenEncoding opt_oglenc = EGL_withoutGL;
  E_EncodingType opt_oenctype = EET_ExplicitLength;
  E_PaddingEncoding opt_opadenc = EPD_withoutPadding;
  OFCmdUnsignedInt opt_filepad = 0;
  OFCmdUnsignedInt opt_itempad = 0;

  // document specific options
  const char *   opt_seriesFile = NULL;
  const char *   opt_patientsName = NULL;
  const char *   opt_patientID = NULL;
  const char *   opt_patientsBirthdate = NULL;
  const char *   opt_documentTitle = NULL;
  const char *   opt_conceptCSD = NULL;
  const char *   opt_conceptCV = NULL;
  const char *   opt_conceptCM = NULL;

  OFBool         opt_readSeriesInfo = OFFalse;
  const char *   opt_patientsSex = NULL;
  OFBool         opt_annotation = OFTrue;
  OFCmdSignedInt opt_instance = 1;
  OFBool         opt_increment = OFFalse;

  OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION , "Convert PDF file to DICOM", rcsid);
  OFCommandLine cmd;
  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);

  cmd.addParam("pdffile-in",  "PDF input filename to be converted");
  cmd.addParam("dcmfile-out", "DICOM output filename");

  cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
   cmd.addOption("--help",                 "-h",     "print this help text and exit", OFCommandLine::AF_Exclusive);
   cmd.addOption("--version",                        "print version information and exit", OFCommandLine::AF_Exclusive);
   cmd.addOption("--arguments",                      "print expanded command line arguments");
   cmd.addOption("--verbose",              "-v",     "verbose mode, print processing details");
   cmd.addOption("--debug",                "-d",     "debug mode, print debug information");

   cmd.addGroup("DICOM document options:");
    cmd.addSubGroup("burned-in annotation:");
      cmd.addOption("--annotation-yes",    "+an",    "PDF contains patient identifying data (default)");
      cmd.addOption("--annotation-no",     "-an",    "PDF does not contain patient identifying data");

    cmd.addSubGroup("document title:");
      cmd.addOption("--title",             "+t",  1, "[t]itle: string (default: empty)",
                                                     "document title");
      cmd.addOption("--concept-name",      "+cn", 3, "[CSD] [CV] [CM]: string (default: empty)",
                                                     "document title as concept name code sequence\n"
                                                     "with coding scheme designator CSD, code value CV\n"
                                                     "and code meaning CM");
    cmd.addSubGroup("patient data:");
      cmd.addOption("--patient-name",      "+pn", 1, "[n]ame: string",
                                                     "patient's name in DICOM PN syntax");
      cmd.addOption("--patient-id",        "+pi", 1, "[i]d: string",
                                                     "patient identifier");
      cmd.addOption("--patient-birthdate", "+pb", 1, "[d]ate: string (YYYYMMDD)",
                                                     "patient's birth date");
      cmd.addOption("--patient-sex",       "+ps", 1, "[s]ex: string (M, F or O)",
                                                     "patient's sex");

    cmd.addSubGroup("study and series:");
      cmd.addOption("--generate",          "+sg",    "generate new study and series UIDs (default)");
      cmd.addOption("--study-from",        "+st", 1, "[f]ilename: string",
                                                     "read patient/study data from DICOM file");
      cmd.addOption("--series-from",       "+se", 1, "[f]ilename: string",
                                                     "read patient/study/series data from DICOM file");
    cmd.addSubGroup("instance number:");
      cmd.addOption("--instance-one",      "+i1",    "use instance number 1 (default, not with +se)");
      cmd.addOption("--instance-inc",      "+ii",    "increment instance number (only with +se)");
      cmd.addOption("--instance-set",      "+is", 1, "[i]nstance number: integer", "use instance number i");

    /* evaluate command line */
    prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);
    if (app.parseCommandLine(cmd, argc, argv, OFCommandLine::PF_ExpandWildcards))
    {
      /* check whether to print the command line arguments */
      if (cmd.findOption("--arguments"))
          app.printArguments();

      /* check exclusive options first */
      if (cmd.hasExclusiveOption())
      {
          if (cmd.findOption("--version"))
          {
              app.printHeader(OFTrue /*print host identifier*/);
              ofConsole.lockCout() << OFendl << "External libraries used:";
              ofConsole.unlockCout();
#ifdef WITH_ZLIB
              ofConsole.lockCout() << OFendl << "- ZLIB, Version " << zlibVersion() << OFendl;
              ofConsole.unlockCout();
#else
              ofConsole.lockCout() << " none" << OFendl;
              ofConsole.unlockCout();
#endif
              return 0;
          }
      }

      /* command line parameters and options */
      cmd.getParam(1, opt_ifname);
      cmd.getParam(2, opt_ofname);

      if (cmd.findOption("--verbose")) opt_verbose = OFTrue;
      if (cmd.findOption("--debug")) opt_debugMode = 5;

      dcmEnableUnknownVRGeneration.set(OFTrue);
      dcmEnableUnlimitedTextVRGeneration.set(OFTrue);

      cmd.beginOptionBlock();
      if (cmd.findOption("--generate"))
      {
        opt_seriesFile = NULL;
        opt_readSeriesInfo = OFFalse;
      }
      if (cmd.findOption("--series-from"))
      {
          app.checkValue(cmd.getValue(opt_seriesFile));
          opt_readSeriesInfo = OFTrue;
      }
      if (cmd.findOption("--study-from"))
      {
          app.checkValue(cmd.getValue(opt_seriesFile));
          opt_readSeriesInfo = OFFalse;
      }
      cmd.endOptionBlock();

      if (cmd.findOption("--title"))
      {
        app.checkValue(cmd.getValue(opt_documentTitle));
      }

      if (cmd.findOption("--concept-name"))
      {
        app.checkValue(cmd.getValue(opt_conceptCSD));
        app.checkValue(cmd.getValue(opt_conceptCV));
        app.checkValue(cmd.getValue(opt_conceptCM));
      }

      if (cmd.findOption("--patient-name"))
      {
        app.checkValue(cmd.getValue(opt_patientsName));
        if (opt_seriesFile) app.printError("--patient-name not allowed with --study-from or --series-from");
      }
      if (cmd.findOption("--patient-id"))
      {
        app.checkValue(cmd.getValue(opt_patientID));
        if (opt_seriesFile) app.printError("--patient-id not allowed with --study-from or --series-from");
      }
      if (cmd.findOption("--patient-birthdate"))
      {
        app.checkValue(cmd.getValue(opt_patientsBirthdate));
        if (opt_seriesFile) app.printError("--patient-birthdate not allowed with --study-from or --series-from");
      }
      if (cmd.findOption("--patient-sex"))
      {
        app.checkValue(cmd.getValue(opt_patientsSex));
        if (opt_seriesFile) app.printError("--patient-sex not allowed with --study-from or --series-from");
      }

      cmd.beginOptionBlock();
      if (cmd.findOption("--annotation-yes"))
      {
        opt_annotation = OFTrue;
      }
      if (cmd.findOption("--annotation-no"))
      {
        opt_annotation = OFTrue;
      }
      cmd.endOptionBlock();

      // initialize default for --series-from
      if (opt_seriesFile && opt_readSeriesInfo) opt_increment = OFTrue;

      cmd.beginOptionBlock();
      if (cmd.findOption("--instance-one"))
      {
        if (opt_seriesFile && opt_readSeriesInfo) app.printError("--instance-one not allowed with --series-from");
        opt_increment = OFFalse;
        opt_instance = 1;
      }
      if (cmd.findOption("--instance-inc"))
      {
        if (!opt_seriesFile || !opt_readSeriesInfo) app.printError("--instance-inc only allowed with --series-from");
        opt_increment = OFTrue;
      }
      if (cmd.findOption("--instance-set"))
      {
        opt_increment = OFFalse;
        app.checkValue(cmd.getValueAndCheckMin(opt_instance, 1));
      }
    }

    if (opt_debugMode)
        app.printIdentifier();
    SetDebugLevel((opt_debugMode));

    /* make sure data dictionary is loaded */
    if (!dcmDataDict.isDictionaryLoaded())
    {
        ofConsole.lockCerr() << "Warning: no data dictionary loaded, "
             << "check environment variable: "
             << DCM_DICT_ENVIRONMENT_VARIABLE << OFendl;
        ofConsole.unlockCerr();
    }

    // read raw file
    if ((opt_ifname == NULL) || (strlen(opt_ifname) == 0))
    {
        ofConsole.lockCerr() << "invalid filename: <empty string>" << OFendl;
        ofConsole.unlockCerr();
        return 1;
    }

    // create study and series UID
    OFString studyUID;
    OFString seriesUID;
    OFString patientsName;
    OFString patientID;
    OFString patientsBirthDate;
    OFString patientsSex;
    Sint32 incrementedInstance = 0;

    if (opt_patientsName) patientsName = opt_patientsName;
    if (opt_patientID) patientID = opt_patientID;
    if (opt_patientsBirthdate) patientsBirthDate = opt_patientsBirthdate;
    if (opt_patientsSex) patientsSex = opt_patientsSex;

    createIdentifiers(opt_readSeriesInfo, opt_seriesFile, studyUID, seriesUID, patientsName, patientID, patientsBirthDate, patientsSex, incrementedInstance);
    if (opt_increment) opt_instance = incrementedInstance;

    if (opt_verbose)
    {
      ofConsole.lockCout() << "creating encapsulated PDF object" << OFendl;
      ofConsole.unlockCout();
    }

    DcmFileFormat fileformat;

    OFCondition result = insertPDFFile(fileformat.getDataset(), opt_ifname, opt_verbose);
    if (result.bad())
    {
         ofConsole.lockCerr() << "unable to create PDF DICOM encapsulation" << OFendl;
         ofConsole.unlockCerr();
    	 return 10;
    }
    if (result.bad()) return 10;

    // now we need to generate an instance number that is guaranteed to be unique within a series.

    result = createHeader(fileformat.getDataset(), patientsName.c_str(), patientID.c_str(),
      patientsBirthDate.c_str(), patientsSex.c_str(), opt_annotation, studyUID.c_str(),
      seriesUID.c_str(), opt_documentTitle, opt_conceptCSD, opt_conceptCV, opt_conceptCM, OFstatic_cast(Sint32, opt_instance));

    if (result.bad())
    {
         ofConsole.lockCerr() << "unable to create DICOM header" << OFendl
             << "Error: " << result.text() << OFendl;
         ofConsole.unlockCerr();
    	 return 10;
    }

    if (opt_verbose)
    {
      ofConsole.lockCout() << "writing encapsulated PDF object as file " << opt_ofname << OFendl;
      ofConsole.unlockCout();
    }

    OFCondition error = EC_Normal;

    if (opt_verbose)
    {
      ofConsole.lockCout() << "Check if new output transfer syntax is possible\n";
      ofConsole.unlockCout();
    }

    DcmXfer opt_oxferSyn(opt_oxfer);

    fileformat.getDataset()->chooseRepresentation(opt_oxfer, NULL);
    if (fileformat.getDataset()->canWriteXfer(opt_oxfer))
    {
        if (opt_verbose)
        {
            ofConsole.lockCout() << "Output transfer syntax " << opt_oxferSyn.getXferName()
                 << " can be written\n";
            ofConsole.unlockCout();
        }
    } else {
        ofConsole.lockCerr() << "No conversion to transfer syntax " << opt_oxferSyn.getXferName()
             << " possible!\n";
        ofConsole.unlockCerr();
        return 1;
    }

    if (opt_verbose)
    {
      ofConsole.lockCout() << "write converted DICOM file with metaheader\n";
      ofConsole.unlockCout();
    }

    error = fileformat.saveFile(opt_ofname, opt_oxfer, opt_oenctype, opt_oglenc,
              opt_opadenc, (Uint32) opt_filepad, (Uint32) opt_itempad);

    if (error.bad())
    {
        ofConsole.lockCerr() << "Error: " << error.text()
             << ": writing file: " << opt_ofname << OFendl;
        ofConsole.unlockCerr();
        return 1;
    }

    if (opt_verbose)
    {
        ofConsole.lockCout() << "conversion successful\n";
        ofConsole.unlockCout();
    }

    return 0;
}


/*
** CVS/RCS Log:
** $Log: pdf2dcm.cc,v $
** Revision 1.10  2009-04-21 14:02:49  joergr
** Fixed minor inconsistencies in manpage / syntax usage.
**
** Revision 1.9  2008-09-25 14:38:48  joergr
** Moved output of resource identifier in order to avoid printing the same
** information twice.
**
** Revision 1.8  2008-09-25 11:19:48  joergr
** Added support for printing the expanded command line arguments.
** Always output the resource identifier of the command line tool in debug mode.
**
** Revision 1.7  2008-04-30 12:38:42  meichel
** Fixed compile errors due to changes in attribute tag names
**
** Revision 1.6  2006/08/15 15:50:56  meichel
** Updated all code in module dcmdata to correctly compile when
**   all standard C++ classes remain in namespace std.
**
** Revision 1.5  2006/07/27 13:52:42  joergr
** Changed parameter "exclusive" of method addOption() from type OFBool into an
** integer parameter "flags". Prepended prefix "PF_" to parseLine() flags.
** Option "--help" is no longer an exclusive option by default.
**
** Revision 1.4  2005/12/08 15:40:53  meichel
** Changed include path schema for all DCMTK header files
**
** Revision 1.3  2005/11/28 15:28:54  meichel
** File dcdebug.h is not included by any other header file in the toolkit
**   anymore, to minimize the risk of name clashes of macro debug().
**
** Revision 1.2  2005/10/26 13:33:49  joergr
** Slightly modified code to use more of the "new" helper functions.
**
** Revision 1.1  2005/10/25 13:01:02  meichel
** Added new tool pdf2dcm that allows to convert PDF files to DICOM
**   Encapsulated PDF Storage SOP instances.
**
*/