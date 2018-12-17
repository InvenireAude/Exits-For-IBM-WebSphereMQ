/*
 * Message Channel Exit - Sniffer (File Logger) for IBM WebSphere MQ
 *
 * Copyright (C) 2015, Invenire Aude, Albert Krzymowski
 *
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/* includes for MQI */
#include <cmqc.h>
#include <cmqxc.h>

#include <chlf.h>

/********************************************************************/
/* Exit Data Structure                                              */
/********************************************************************/
#define IAE_FILENAME_LEN 		256
#define IAE_DATAPATH 			"/var/mqm/iae/data"
#define IAE_DFTLOG             10*1024*1024
#define IAE_VERSION            "1.0.2"

typedef struct _ExitData{

	int   iFD;
	int   iBytesWritten;
	int   iMaxFileSize;

	FILE *fDebug;

	int  bVerbose;

    char sActiveFileName[IAE_FILENAME_LEN];
	char sMsgUserData[MQ_EXIT_DATA_LENGTH+1];
	char sChannelName[MQ_CHANNEL_NAME_LENGTH+1];
	char sQMgrName[MQ_Q_MGR_NAME_LENGTH+1];

	int  iErrors;
	int  bCloseChlOnError;

	time_t iStartTS;

}ExitData;

static ExitData DftExitData = { -1, 0, 10000, NULL, 0, "", "", 0, 0 };

/********************************************************************/
 /* Feedback codes                                                   */
 /********************************************************************/

#define IAE_FB_INVALIDEXITID       0x00110000
#define IAE_FB_INVALIDEXITREASON   0x00120000
#define IAE_FB_PROCESSINGERROR     0x00130000

#define IAE_FB_DEMOEXPIRED         0x00ffffff
/********************************************************************/
static char  ExitReasons[][14] = {"MQXR_INIT"
                          ,"MQXR_TERM"
                          ,"MQXR_MSG"
                          ,"MQXR_XMIT"
                          ,"MQXR_SEC_MSG"
                          ,"MQXR_INIT_SEC"
                          ,"MQXR_RETRY"
                          };

/********************************************************************/
void prefixTime(FILE *f){

	time_t iTime= time(NULL);
	struct tm tmTime;

	localtime_r(&iTime,&tmTime);

	fprintf(f,"%4d-%02d-%02d %02d:%02d:%02d IAE: ",
			1900 + tmTime.tm_year,
			tmTime.tm_mon,
			tmTime.tm_mday,
			tmTime.tm_hour,
			tmTime.tm_min,
			tmTime.tm_sec);

}
/********************************************************************/
#define LOG(d, ... ){\
	if((d) != NULL && (d)->fDebug != NULL){ prefixTime((d)->fDebug);\
	   fprintf((d)->fDebug, __VA_ARGS__ ); fflush((d)->fDebug); } }

#define DBG(d, ... ){\
	if((d) != NULL && (d)->fDebug != NULL && (d)->bVerbose){ prefixTime((d)->fDebug);\
	   fprintf((d)->fDebug, __VA_ARGS__ ); fflush((d)->fDebug); } }

/********************************************************************/
static void mqstrcpy(char *d, const MQCHAR *s, int n){

	while(n-- && *s && *s != ' '){

		 char c = (char) *s++;

		 if(isalpha(c) || isdigit(c))
			 *d++=c;
		 else
			 *d++='_';

	}

	*d=0;
}
/********************************************************************/
static void openOrCreateFile(ExitData *pExitData){

	if(!pExitData)
		return;

		int iResult = -1;

		iResult = open(pExitData->sActiveFileName, O_CREAT | O_WRONLY,  0666);

		if(iResult == -1){
			LOG(pExitData,"Cannot open file:[%s], errno=[%d]\n",pExitData->sActiveFileName,errno);
			return;
		}

		pExitData->iFD=iResult;

		off_t iBytesWritten = lseek(iResult, 0L, SEEK_END);

		if(iBytesWritten < 0){
			LOG(pExitData,"Cannot seek in file:[%s], errno=[%d]\n",pExitData->sActiveFileName,errno);
			return;
		}

		pExitData->iBytesWritten = iBytesWritten;

		LOG(pExitData,"Active file has %d bytes.\n",iBytesWritten);

		return;

}
/********************************************************************/
static void checkAndRollFile(ExitData *pExitData, MQLONG iDataLen){

	if(!pExitData)
		return;

	 char sFileNameBuf[IAE_FILENAME_LEN];
	 int iResult = -1;

 	 DBG(pExitData,"Check %d, %d, %d\n",pExitData->iBytesWritten,pExitData->iMaxFileSize,iDataLen);

	 if(pExitData->iBytesWritten + iDataLen <= pExitData->iMaxFileSize)
		return;

	 if(pExitData->iFD == -1){
	 	 LOG(pExitData,"IAE: ERROR (checkAndRollFile) File is not opened !\n");
	 	 return;
	 }

	 time_t iTime= time(NULL);
	 struct tm tmTime;

	 localtime_r(&iTime,&tmTime);

	 snprintf(sFileNameBuf, IAE_FILENAME_LEN, "%s/%s_%s-%4d%02d%02d-%02d%02d%02d.dat",
			    IAE_DATAPATH,
				pExitData->sQMgrName,
				pExitData->sChannelName,
				1900 + tmTime.tm_year,
				tmTime.tm_mon,
				tmTime.tm_mday,
				tmTime.tm_hour,
				tmTime.tm_min,
				tmTime.tm_sec);

	 close(pExitData->iFD);
	 rename(pExitData->sActiveFileName, sFileNameBuf);

	 openOrCreateFile(pExitData);

	return;

}
/********************************************************************/
static int writeToFile(ExitData *pExitData, const char* pData, int iDataLength){

	while (iDataLength > 0) {

		MQLONG iWritten = write(pExitData->iFD, pData, iDataLength);

		if (iWritten == -1) {
			LOG(pExitData, "IAE: ERROR (write) errno:%d\n", errno);
			return 0;
		}

		pExitData->iBytesWritten+=iWritten;
		pData += iWritten;
		iDataLength -= iWritten;
	}

	return 1;
}
/********************************************************************/
static void writeData(ExitData *pExitData, PMQCHAR  pAgentBuffer, MQLONG iDataLength){

	if(!pExitData)
		return;

	IAE_CHFL_Header header;

	header.iDataLength = iDataLength;
	header.iTimestamp  = time(NULL);

	if(pExitData->iFD == -1){
	 	 LOG(pExitData,"IAE: ERROR (writeData) File is not opened !\n");
	 	 return;
	}

	DBG(pExitData,"IAE: Writing %d bytes.\n",iDataLength);

	if(!writeToFile(pExitData, (const char*)&header, sizeof(IAE_CHFL_Header)))
		return;

	writeToFile(pExitData, pAgentBuffer, iDataLength);

}
/********************************************************************/
int getLogMaxSize(const char* s){

	int iLogSize = 0;

	int iNumDigits = 3;

	while(iNumDigits--){
		if(isdigit(*s))
			iLogSize += *s++  - '0';
		else
			return IAE_DFTLOG;

		if(iNumDigits)
			iLogSize*=10;
	}

	iLogSize<<=20;

	return iLogSize;
}
/********************************************************************/
static ExitData *createExitData(const MQCHAR *sMsgUserData, const MQCHAR* sChannelName, const MQCHAR* sQMgrName){

	ExitData *pResult = malloc(sizeof(ExitData));

	if(!pResult)
		goto out;

	*pResult=DftExitData;

	mqstrcpy(pResult->sMsgUserData, sMsgUserData, MQ_EXIT_DATA_LENGTH);
	mqstrcpy(pResult->sChannelName, sChannelName, MQ_CHANNEL_NAME_LENGTH);
	mqstrcpy(pResult->sQMgrName,    sQMgrName,    MQ_Q_NAME_LENGTH);

	if(pResult->sMsgUserData[0] == 'D' || pResult->sMsgUserData[0] == 'd'){
		char sDbgFileName[256];

		snprintf(sDbgFileName, IAE_FILENAME_LEN, "/var/mqm/iae/errors/iae.%s_%s.%d.txt",
				pResult->sQMgrName, pResult->sChannelName, getpid());


		pResult->fDebug=fopen(sDbgFileName,"a");

		pResult->bVerbose = pResult->sMsgUserData[0] == 'D';

		LOG(pResult,"+--------------------------------------------------------------+\n");
		LOG(pResult,"| Message Channel Exit by www.invenireaude.com                 |\n");
		LOG(pResult,"+--------------------------------------------------------------+\n");
		LOG(pResult,"| Version: %10s                                                  |\n", IAE_VERSION );
		LOG(pResult,"| More info: http://www.invenireaude.com/article;id=exits.chfl   |\n");
		LOG(pResult,"+--------------------------------------------------------------+\n\n");
		LOG(pResult,"Exit Initialized for the [%s] channel, parameters are [%s].\n", pResult->sChannelName, pResult->sMsgUserData);
	}

	if(pResult->sMsgUserData[1] == 'C' || pResult->sMsgUserData[1] == 'c')
		pResult->bCloseChlOnError = 1;

	LOG(pResult,"On error: %s.\n", pResult->bCloseChlOnError ? "close channel" :  "suppress exit function");

	pResult->iMaxFileSize = getLogMaxSize(pResult->sMsgUserData+2);

	LOG(pResult,"Max log file size: %d.\n", pResult->iMaxFileSize);

	snprintf(pResult->sActiveFileName,IAE_FILENAME_LEN, "%s/%s_%s-active.dat", IAE_DATAPATH,
			 pResult->sQMgrName, pResult->sChannelName);

	LOG(pResult,"Active file: %s.\n", pResult->sActiveFileName);

	openOrCreateFile(pResult);

out:
	return pResult;
}
/********************************************************************/
static void destroyExitData(ExitData **pPtrExitData){

	if(*pPtrExitData == NULL)
		return;

	LOG((*pPtrExitData),"IAE: Exit Terminated.\n");

	if((*pPtrExitData)->iFD)
		close((*pPtrExitData)->iFD);

	if((*pPtrExitData)->fDebug)
		fclose((*pPtrExitData)->fDebug);

	free(*pPtrExitData);
	*pPtrExitData=0;
}
/********************************************************************/
/*                   M A I N   E X I T                              */
/********************************************************************/

void MQENTRY iaechfl(PMQCXP   pExitParms
                    ,PMQCD    pChannelDef
                    ,PMQLONG  pDataLength
                    ,PMQLONG  pAgentBufferLength
                    ,PMQCHAR  pAgentBuffer
                    ,PMQLONG  pExitBufferLength
                    ,PMQCHAR *pExitBuffer
                    ){

	 MQLONG    FeedbackCode = 0;

	 pExitParms -> ExitResponse = MQXCC_OK;

	 ExitData **pPtrExitData = (ExitData **)(&(pExitParms->ExitUserArea));

	 if(pExitParms -> ExitId != MQXT_CHANNEL_MSG_EXIT){
		 FeedbackCode = IAE_FB_INVALIDEXITID + pExitParms->ExitId;
		 goto out;
	 }

	 switch(pExitParms -> ExitReason){

	 	 case MQXR_INIT:
	    	 *pPtrExitData = createExitData(pChannelDef->MsgUserData,
	    			 	 	 	 	 	    pChannelDef->ChannelName,
											pChannelDef->QMgrName);
	    	 goto out;
	         break;

	    case MQXR_TERM:
	    	 destroyExitData(pPtrExitData);
	         goto out;
	         break;

	    case MQXR_MSG:
	    	 checkAndRollFile(*pPtrExitData,*pDataLength);
	    	 writeData(*pPtrExitData,pAgentBuffer,*pDataLength);
	         break;

	    default:
			 LOG(*pPtrExitData,"Invalid reason [%s].\n", ExitReasons[pExitParms->ExitReason-MQXR_INIT]);
	         FeedbackCode = IAE_FB_INVALIDEXITREASON + pExitParms->ExitReason;
	         goto out;
	         break;
	  }

 out:
	  if(FeedbackCode){
	     pExitParms -> ExitResponse =  MQXCC_SUPPRESS_FUNCTION;
	     pExitParms -> Feedback     =  FeedbackCode;
	  }

	  if((*pPtrExitData)->iErrors){

		  if((*pPtrExitData)->bCloseChlOnError){
			   LOG((*pPtrExitData),"Request to close channel due to some errors. ");
			   pExitParms -> ExitResponse =  MQXCC_CLOSE_CHANNEL;
		  }else{
			   LOG((*pPtrExitData),"Request to suppress channel exit due to some errors. ");
			   pExitParms -> ExitResponse =  MQXCC_SUPPRESS_FUNCTION;
		  }

		  pExitParms -> Feedback = IAE_FB_PROCESSINGERROR;
	  }

}
/********************************************************************/
#if MQAT_DEFAULT == MQAT_UNIX
 void MQStart(){;}
#endif
