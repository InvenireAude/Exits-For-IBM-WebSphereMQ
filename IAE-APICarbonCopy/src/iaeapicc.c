/*
 * API Carbon Copy Exit - iaeapicc.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmqec.h>

/********************************************************************/
#define IAE_VERSION "1.0.8"
#define IAE_FILENAME_LEN 		256
/*********************************************************************/

const char * IAE_FunctionId ( MQLONG Function){
  char *p = NULL;

  switch (Function)
  {
  case MQXF_INIT:             p = "MQXF_INIT";             break;
  case MQXF_TERM:             p = "MQXF_TERM";             break;
  case MQXF_CONN:             p = "MQXF_CONN";             break;
  case MQXF_CONNX:            p = "MQXF_CONNX";            break;
  case MQXF_DISC:             p = "MQXF_DISC";             break;
  case MQXF_OPEN:             p = "MQXF_OPEN";             break;
  case MQXF_SUB:              p = "MQXF_SUB";              break;
  case MQXF_SUBRQ:            p = "MQXF_SUBRQ";            break;
  case MQXF_CB:               p = "MQXF_CB";               break;
  case MQXF_CTL:              p = "MQXF_CTL";              break;
  case MQXF_CALLBACK:         p = "MQXF_CALLBACK";         break;
  case MQXF_CLOSE:            p = "MQXF_CLOSE";            break;
  case MQXF_PUT1:             p = "MQXF_PUT1";             break;
  case MQXF_PUT:              p = "MQXF_PUT";              break;
  case MQXF_GET:              p = "MQXF_GET";              break;
  case MQXF_DATA_CONV_ON_GET: p = "MQXF_DATA_CONV_ON_GET"; break;
  case MQXF_INQ:              p = "MQXF_INQ";              break;
  case MQXF_SET:              p = "MQXF_SET";              break;
  case MQXF_BEGIN:            p = "MQXF_BEGIN";            break;
  case MQXF_CMIT:             p = "MQXF_CMIT";             break;
  case MQXF_BACK:             p = "MQXF_BACK";             break;
  case MQXF_STAT:             p = "MQXF_STAT";             break;
  case MQXF_XACLOSE:          p = "MQXF_XACLOSE";          break;
  case MQXF_XACOMMIT:         p = "MQXF_XACOMMIT";         break;
  case MQXF_XACOMPLETE:       p = "MQXF_XACOMPLETE";       break;
  case MQXF_XAEND:            p = "MQXF_XAEND";            break;
  case MQXF_XAFORGET:         p = "MQXF_XAFORGET";         break;
  case MQXF_XAOPEN:           p = "MQXF_XAOPEN";           break;
  case MQXF_XAPREPARE:        p = "MQXF_XAPREPARE";        break;
  case MQXF_XARECOVER:        p = "MQXF_XARECOVER";        break;
  case MQXF_XAROLLBACK:       p = "MQXF_XAROLLBACK";       break;
  case MQXF_XASTART:          p = "MQXF_XASTART";          break;
  case MQXF_AXREG:            p = "MQXF_AXREG";            break;
  case MQXF_AXUNREG:          p = "MQXF_AXUNREG";          break;
  default:
    p = "Unknown MQXF";
  }

  return p;
}
/********************************************************************/
typedef struct {

	MQCHAR sPutPrefix[MQ_Q_NAME_LENGTH];
	MQCHAR sGetPrefix[MQ_Q_NAME_LENGTH];

	int  bResolveAlias;

	FILE *fDebug;

	int bVerbose;
	int iNumMsgs;

	int bOverrideExpiration;
	int bOverridePersistance;

	MQLONG iPersistance;

} IAE_ExitUserArea;
/********************************************************************/
static IAE_ExitUserArea DftExitUserArea = {
		"",
		"",
		0,
		0,

		0,
		0,

		0,
		0,

		0
};
/********************************************************************/
void prefixTime(FILE *f, const char *sFun, int iLine){

	time_t iTime= time(NULL);
	struct tm tmTime;

	localtime_r(&iTime,&tmTime);

	fprintf(f,"%4d-%02d-%02d %02d:%02d:%02d IAE: %20s(%3d) : ",
			1900 + tmTime.tm_year,
			tmTime.tm_mon,
			tmTime.tm_mday,
			tmTime.tm_hour,
			tmTime.tm_min,
			tmTime.tm_sec,
			sFun,iLine);

}
/********************************************************************/
#define LOG(d, ... ){\
	if((d) != NULL && (d)->fDebug != NULL){ prefixTime((d)->fDebug,__FUNCTION__,__LINE__);\
	   fprintf((d)->fDebug, __VA_ARGS__ ); fflush((d)->fDebug); } }

#define DBG(d, ... ){\
	if((d) != NULL && (d)->fDebug != NULL && (d)->bVerbose){ prefixTime((d)->fDebug, __FUNCTION__,__LINE__);\
	   fprintf((d)->fDebug, __VA_ARGS__ ); fflush((d)->fDebug); } }

/********************************************************************/
static IAE_ExitUserArea *IAE_CreateExitData(const char *sParameters){


	//BUG in WMQ clone() ?
	//IAE_ExitUserArea *pResult = malloc(sizeof(IAE_ExitUserArea));

	static IAE_ExitUserArea _data;
	IAE_ExitUserArea *pResult = &_data;

	;
	if(!pResult)
		goto out;

	*pResult=DftExitUserArea;

	int bFirst = 1;

	while(*sParameters > ' '){

		if(bFirst)
			bFirst = 0;
		else if(*sParameters++ != ',')
				goto error;


		char cOption = *sParameters++;

		if(*sParameters++ != '=' || *sParameters <= ' ')
			goto error;

		switch(cOption){

			case 'D':

				if(*sParameters == 'Y' || *sParameters == 'y'){
					char sDbgFileName[256];

					snprintf(sDbgFileName, IAE_FILENAME_LEN, "/var/mqm/iae/errors/iae.api.%d.txt",getpid());

					pResult->fDebug=fopen(sDbgFileName,"a");

					pResult->bVerbose = *sParameters == 'Y';

					LOG(pResult,"+--------------------------------------------------------------+\n");
					LOG(pResult,"| Carbon Copy Exit by Invenire Aude (www.invenireaude.com)     |\n");
					LOG(pResult,"+--------------------------------------------------------------+\n");
					LOG(pResult,"| Version: %10s                                         |\n", IAE_VERSION );
					LOG(pResult,"| More info: http://www.invenireaude.com/article;id=exits.cc   |\n");
					LOG(pResult,"+--------------------------------------------------------------+\n\n");
				}else{
					goto error;
				}
				break;

			case 'E':
				if(*sParameters == 'Y' || *sParameters == 'y')
					pResult->bOverrideExpiration = 1;
				else if(*sParameters != 'N' && *sParameters != 'n'){
					LOG(pResult, "Bad value for 'Override Expiration (E)': (Y/y/N/n) given '%c'\n",*sParameters);
					goto error;
				}
				break;

			case 'A':
				if(*sParameters == 'Y' || *sParameters == 'y')
					pResult->bResolveAlias = 1;
				else if(*sParameters != 'N' && *sParameters != 'n'){
					LOG(pResult, "Bad value for 'Resolve Alias (A)': (Y/y/N/n) given '%c'\n",*sParameters);
					goto error;
				}
				break;

			case 'p':

				{
					char *sTmp=pResult->sPutPrefix;

					while(*sParameters > ' ' && *sParameters != ',')
						*sTmp++=*sParameters++;
					sTmp=0;
					sParameters--;
				}
				break;

			case 'g':

					{
						char *sTmp=pResult->sGetPrefix;

						while(*sParameters > ' ' && *sParameters != ',')
							*sTmp++=*sParameters++;
						sTmp=0;
						sParameters--;
					}

					break;

			case 'P':
				if(*sParameters == '0' || *sParameters == '1' || *sParameters == '2'){
					pResult->bOverridePersistance = 1;
					pResult->iPersistance = *sParameters - '0';
				} else{
					LOG(pResult, "Bad value for 'Presistance (P)': (0/1/2) given '%c'\n",*sParameters);
					goto error;
				}

				break;

			default:
				goto error;

		}

		sParameters++;
	}

	if(!*(pResult->sPutPrefix) && !*(pResult->sGetPrefix)){
		LOG(pResult,"Specify at least one queue prefix for MQGET or MQPUT calls.");
		goto error;
	}

	LOG(pResult,"Override expiration:  %s.\n", pResult->bOverrideExpiration ? "YES" : "NO");
	LOG(pResult,"Override presistance: %s.\n", pResult->bOverridePersistance ? "YES" : "NO");
	LOG(pResult,"Resolve alias:        %s.\n", pResult->bResolveAlias ? "YES" : "NO");
	LOG(pResult,"Queue prefix MQPUT:     [%s]\n", pResult->sPutPrefix );
	LOG(pResult,"Queue prefix MQGET:     [%s]\n", pResult->sGetPrefix );

out:
	return pResult;

error:

	if(pResult)
		free(pResult);

	return 0;
}
/*********************************************************************/
int IAE_GetObjectName(IAE_ExitUserArea  * pExitUserArea,
					  PMQAXP    pExitParms,
					  MQHCONN   pHconn,
	    			  MQHOBJ    pHobj,
					  PMQCHAR   sQueueName){

	MQLONG cc;
	MQLONG rc;
	MQLONG tabSelectors[2];
	MQLONG iQueueType;

	tabSelectors[0] = MQCA_Q_NAME;
	tabSelectors[1] = MQIA_Q_TYPE;

	pExitParms->Hconfig->MQINQ_Call(pHconn,
		  pHobj,
		  2,
		  tabSelectors,
		  1, &iQueueType,
		  MQ_Q_NAME_LENGTH, sQueueName,
		  &cc, &rc);

	DBG(pExitUserArea,"calling MQINQ (...), cc=%d,rc=%d\n",cc,rc);

	if(rc == MQCC_OK){
		DBG(pExitUserArea,"QNAME:[%.*s],type= %d\n", MQ_Q_NAME_LENGTH, sQueueName, iQueueType);
	}

	if(iQueueType == MQQT_ALIAS && pExitUserArea->bResolveAlias){

		tabSelectors[0] = MQCA_BASE_Q_NAME;

		pExitParms->Hconfig->MQINQ_Call(pHconn,
			  pHobj,
			  1,
			  tabSelectors,
			  0,0,
			  MQ_Q_NAME_LENGTH, sQueueName,
			  &cc, &rc);

		if(rc == MQCC_OK){
			DBG(pExitUserArea,"BaseQ:[%.*s]\n", MQ_Q_NAME_LENGTH, sQueueName);
		}

	}

	return rc == MQCC_OK;
}
/*********************************************************************/
int IAE_GetAliasName(IAE_ExitUserArea  * pExitUserArea,
					  PMQAXP    pExitParms,
					  MQHCONN   pHconn,
					  PMQCHAR   sAliasQName,
					  PMQCHAR   sQueueName){

	MQLONG cc;
	MQLONG rc;
	MQLONG tabSelectors[2];
	MQLONG iQueueType;
	MQOD   od = { MQOD_DEFAULT };
	MQLONG iOptions = MQOO_INQUIRE;
	MQHOBJ pHobj = MQHO_NONE;


	pExitParms->Hconfig->MQOPEN_Call(pHconn,
		   &od,
		   iOptions,
		   &pHobj,&cc,&rc);

	if(cc == MQCC_OK){
		IAE_GetObjectName(pExitUserArea,pExitParms,pHconn,pHobj,sQueueName);
	}else
		return 0;

	pExitParms->Hconfig->MQCLOSE_Call(pHconn,
			   &pHobj,
			   0,&cc,&rc);

	return rc == MQCC_OK;
}
/*********************************************************************/
void IAE_MakeCCAfterGet(IAE_ExitUserArea  * pExitUserArea,
						PMQAXP    pExitParms,
						PMQHCONN  pHconn,
					    PMQHOBJ   pHobj,
						PPMQMD    ppMsgDesc,
						PPMQGMO   ppGetMsgOpts,
						MQLONG    iBufferLength,
						PPMQVOID  ppBuffer,
						MQLONG    iDataLength,
						MQLONG    iCompCode,
						MQLONG    iReason){

	MQLONG iGetOptions = (*ppGetMsgOpts)->Options;

	MQMD2  md;
	MQOD   od = { MQOD_DEFAULT };
	MQCHAR sQueueName[MQ_Q_NAME_LENGTH];
	MQCHAR sOutputName[MQ_Q_NAME_LENGTH * 2];
	MQPMO  pmo = { MQPMO_DEFAULT };
	MQLONG cc;
	MQLONG rc;

static	MQLONG iSkipOptions = MQGMO_BROWSE_CO_OP |
		MQGMO_BROWSE_FIRST | MQGMO_BROWSE_NEXT |MQGMO_BROWSE_MSG_UNDER_CURSOR |
		MQGMO_BROWSE_HANDLE;

	MQLONG iOutputDataLength = iDataLength;

#ifdef IAE_DEMO

	pExitUserArea->iNumMsgs++;
	LOG(pExitUserArea," DEMO %d of 100 \n", pExitUserArea->iNumMsgs);

	if(pExitUserArea->iNumMsgs > 100)
		return;


#endif

	if(iBufferLength < iOutputDataLength)
		iOutputDataLength = iBufferLength;

	if(iCompCode == MQCC_FAILED || (iGetOptions & iSkipOptions))
		  return;

	if(!IAE_GetObjectName(pExitUserArea,pExitParms,*pHconn,*pHobj,sQueueName))
		return;

	if(!*(pExitUserArea->sGetPrefix))
		return;

	if(strncmp(sQueueName,pExitUserArea->sGetPrefix,strnlen(pExitUserArea->sGetPrefix, MQ_OBJECT_NAME_LENGTH)) == 0)
		return;

	strncpy(sOutputName,pExitUserArea->sGetPrefix,MQ_Q_NAME_LENGTH);
	strncat(sOutputName,sQueueName,MQ_Q_NAME_LENGTH);

	memcpy((void*)(od.ObjectName), sOutputName, MQ_OBJECT_NAME_LENGTH);

	DBG(pExitUserArea,"Trace to: %.*s\n", MQ_OBJECT_NAME_LENGTH, od.ObjectName);

	if( (*ppMsgDesc)->Version == MQMD_VERSION_1)
		memcpy(&md,*ppMsgDesc,sizeof(MQMD1));
	else if( (*ppMsgDesc)->Version == MQMD_VERSION_2)
		memcpy(&md,*ppMsgDesc,sizeof(MQMD2));

	if(pExitUserArea->bOverrideExpiration)
		md.Expiry=MQEI_UNLIMITED;

	if(pExitUserArea->bOverridePersistance)
		md.Persistence=pExitUserArea->iPersistance;

	if(iGetOptions & MQGMO_SYNCPOINT)
		pmo.Options |= MQPMO_SYNCPOINT;

	if((iGetOptions & MQGMO_SYNCPOINT_IF_PERSISTENT) && md.Persistence == MQPER_PERSISTENT)
		pmo.Options |= MQPMO_SYNCPOINT;

     DBG(pExitUserArea,"Version: %d\n", pExitParms->Version);

	 if((*ppGetMsgOpts)->Version >= MQGMO_VERSION_3){

	    if( ((*ppGetMsgOpts)->MsgHandle != MQHM_NONE)  &&
	    	((*ppGetMsgOpts)->MsgHandle != MQHM_UNUSABLE_HMSG) ){

		     DBG(pExitUserArea,"Handle: Setting up !\n", (*ppGetMsgOpts)->MsgHandle);

	 		pmo.NewMsgHandle=(*ppGetMsgOpts)->MsgHandle;
			pmo.Version = MQPMO_VERSION_3;
			pmo.Action  = MQACTP_NEW;
	    }
	 }

	 pExitParms->Hconfig->MQPUT1_Call(*pHconn,
			&od,
			&md,
			&pmo,
			iOutputDataLength,
			*ppBuffer,
			&cc,
			&rc);

	DBG(pExitUserArea,"calling MQPUT1(...) cc=%ld,rc=%ld \n",cc,rc);

}
/*********************************************************************/
/* After MQGET Entrypoint                                            */
/*********************************************************************/

MQ_GET_EXIT GetAfter;

void MQENTRY GetAfter    (  PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQHCONN  pHconn
                           , PMQHOBJ   pHobj
                           , PPMQMD    ppMsgDesc
                           , PPMQGMO   ppGetMsgOpts
                           , PMQLONG   pBufferLength
                           , PPMQVOID  ppBuffer
                           , PPMQLONG  ppDataLength
                           , PMQLONG   pCompCode
                           , PMQLONG   pReason
                         ){
	IAE_ExitUserArea ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
	IAE_ExitUserArea  * pExitUserArea  = *ppExitUserArea;

	if(!pExitUserArea)
		return;

	DBG(pExitUserArea," >> \n");

	if(*(pExitUserArea->sGetPrefix))
		IAE_MakeCCAfterGet(pExitUserArea,pExitParms,pHconn,pHobj,ppMsgDesc,ppGetMsgOpts,*pBufferLength,ppBuffer,**ppDataLength,*pCompCode,*pReason);

	DBG(pExitUserArea," << \n");

  return;
}


/*********************************************************************/
/* Before MQOPEN Entrypoint                                          */
/*********************************************************************/

MQ_OPEN_EXIT OpenBefore;

void MQENTRY OpenBefore  ( PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQHCONN  pHconn
                           , PPMQOD    ppObjDesc
                           , PMQLONG   pOptions
                           , PPMQHOBJ  ppHobj
                           , PMQLONG   pCompCode
                           , PMQLONG   pReason
                         ){
	IAE_ExitUserArea ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
	IAE_ExitUserArea  * pExitUserArea  = *ppExitUserArea;

	if(!pExitUserArea)
		return;

	DBG(pExitUserArea," >> \n");

	if(ppObjDesc &&  *ppObjDesc)
		DBG(pExitUserArea,"calling MQOPEN('%.*s',...) \n",MQ_Q_NAME_LENGTH, (*ppObjDesc)->ObjectName);


	if(*(pExitUserArea->sGetPrefix))
		if(pOptions)
			if( (*pOptions & MQOO_INPUT_AS_Q_DEF) ||
				(*pOptions & MQOO_INPUT_SHARED)   ||
				(*pOptions & MQOO_INPUT_EXCLUSIVE)){
				*pOptions |= MQOO_INQUIRE;
			}

	if(*(pExitUserArea->sPutPrefix))
		if(pOptions)
				if( (*pOptions & MQOO_OUTPUT)){
					*pOptions |= MQOO_INQUIRE;
				}

	DBG(pExitUserArea," << \n");

}
/*********************************************************************/
/* After MQ Callback Entrypoint                                      */
/*********************************************************************/

MQ_CALLBACK_EXIT CallbackAfter;

void MQENTRY CallbackAfter ( PMQAXP    pExitParms
                             , PMQAXC    pExitContext
                             , PMQHCONN  pHconn
                             , PPMQMD    ppMsgDesc
                             , PPMQGMO   ppGetMsgOpts
                             , PPMQVOID  ppBuffer
                             , PPMQCBC   ppMQCBContext
                           ){

	IAE_ExitUserArea ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
	IAE_ExitUserArea  * pExitUserArea  = *ppExitUserArea;

	DBG(pExitUserArea," >> \n");


	if(*(pExitUserArea->sGetPrefix))
		IAE_MakeCCAfterGet(pExitUserArea, pExitParms, pHconn, &((*ppMQCBContext)->Hobj),
			ppMsgDesc, ppGetMsgOpts, (*ppMQCBContext)->BufferLength, ppBuffer, (*ppMQCBContext)->DataLength,
			(*ppMQCBContext)->CompCode, (*ppMQCBContext)->Reason);

	DBG(pExitUserArea," << \n");

 }

/*********************************************************************/
void IAE_MakeCCAfterPut(IAE_ExitUserArea  * pExitUserArea,
								PMQAXP    pExitParms,
								PMQHCONN  pHconn,
								PMQCHAR   sQueueName,
		                        PMQMD     pMsgDesc,
		                        PMQPMO    pPutMsgOpts,
		                        MQLONG    iDataLength,
		                        PMQVOID   pBuffer,
								MQLONG    iCompCode,
								MQLONG    iReason){

	MQMD2  md;
	MQOD   od = { MQOD_DEFAULT };
	MQCHAR sOutputName[MQ_Q_NAME_LENGTH * 2];
	MQLONG cc;
	MQLONG rc;

#ifdef IAE_DEMO

	pExitUserArea->iNumMsgs++;
	LOG(pExitUserArea," DEMO %d of 100 \n", pExitUserArea->iNumMsgs);

	if(pExitUserArea->iNumMsgs > 100)
		return;

#endif

	if(strncmp(sQueueName,pExitUserArea->sPutPrefix,strnlen(pExitUserArea->sPutPrefix, MQ_OBJECT_NAME_LENGTH)) == 0)
		return;

	strncpy(sOutputName,pExitUserArea->sPutPrefix,MQ_Q_NAME_LENGTH);
	strncat(sOutputName,sQueueName,MQ_Q_NAME_LENGTH);

	memcpy((void*)(od.ObjectName), sOutputName, MQ_OBJECT_NAME_LENGTH);

	DBG(pExitUserArea,"Trace to: %.*s\n", MQ_OBJECT_NAME_LENGTH, od.ObjectName);

	if( (pMsgDesc)->Version == MQMD_VERSION_1)
		memcpy(&md,pMsgDesc,sizeof(MQMD1));
	else if( (pMsgDesc)->Version == MQMD_VERSION_2)
		memcpy(&md,pMsgDesc,sizeof(MQMD2));

	if(pExitUserArea->bOverrideExpiration)
		md.Expiry=MQEI_UNLIMITED;

	if(pExitUserArea->bOverridePersistance)
		md.Persistence=pExitUserArea->iPersistance;

	 pExitParms->Hconfig->MQPUT1_Call(*pHconn,
			&od,
			&md,
			pPutMsgOpts,
			iDataLength,
			pBuffer,
			&cc,
			&rc);

	DBG(pExitUserArea,"calling MQPUT1(...) cc=%ld,rc=%ld \n",cc,rc);

}
/*********************************************************************/
/* After MQPUT Entrypoint                                            */
/*********************************************************************/

MQ_PUT_EXIT PutAfter;

void MQENTRY PutAfter    ( PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQHCONN  pHconn
                           , PMQHOBJ   pHobj
                           , PPMQMD    ppMsgDesc
                           , PPMQPMO   ppPutMsgOpts
                           , PMQLONG   pBufferLength
                           , PPMQVOID  ppBuffer
                           , PMQLONG   pCompCode
                           , PMQLONG   pReason
                         ){

	MQCHAR sQueueName[MQ_Q_NAME_LENGTH];

	IAE_ExitUserArea ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
	IAE_ExitUserArea  * pExitUserArea  = *ppExitUserArea;

	if(!pExitUserArea)
		return;

	DBG(pExitUserArea," >> \n");

	if(*pCompCode == MQCC_FAILED)
		  return;

	if(!IAE_GetObjectName(pExitUserArea,pExitParms,*pHconn,*pHobj,sQueueName))
			return;

	IAE_MakeCCAfterPut(pExitUserArea, pExitParms, pHconn,
			sQueueName, *ppMsgDesc, *ppPutMsgOpts, *pBufferLength,
			*ppBuffer, *pCompCode, *pReason);

	DBG(pExitUserArea," << \n");

}
/*********************************************************************/
/* After MQPUT1 Entrypoint                                           */
/*********************************************************************/

MQ_PUT1_EXIT Put1After;

void MQENTRY Put1After   ( PMQAXP    pExitParms
                           , PMQAXC    pExitContext
                           , PMQHCONN  pHconn
                           , PPMQOD    ppObjDesc
                           , PPMQMD    ppMsgDesc
                           , PPMQPMO   ppPut1MsgOpts
                           , PMQLONG   pBufferLength
                           , PPMQVOID  ppBuffer
                           , PMQLONG   pCompCode
                           , PMQLONG   pReason
                         ){

	MQCHAR sQueueName[MQ_Q_NAME_LENGTH];

	IAE_ExitUserArea ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
	IAE_ExitUserArea  * pExitUserArea  = *ppExitUserArea;

	if(!pExitUserArea)
		return;

	DBG(pExitUserArea," >> \n");

	if(*pCompCode == MQCC_FAILED)
		  return;

	if(!pExitUserArea->bResolveAlias){
		memcpy(sQueueName, (*ppObjDesc)->ResolvedQName, MQ_Q_NAME_LENGTH);
		DBG(pExitUserArea," Using: [%.*s] \n",MQ_Q_NAME_LENGTH,sQueueName);
	}else{
		if(!IAE_GetAliasName(pExitUserArea,pExitParms,*pHconn,(*ppObjDesc)->ResolvedQName,sQueueName))
			return;
	}

	IAE_MakeCCAfterPut(pExitUserArea, pExitParms, pHconn,
			(*ppObjDesc)->ObjectName, *ppMsgDesc, *ppPut1MsgOpts, *pBufferLength,
			*ppBuffer, *pCompCode, *pReason);


	DBG(pExitUserArea," << \n");

}
/*********************************************************************/
/*                                                                   */
/* TERM                                                              */
/*                                                                   */
/*********************************************************************/

MQ_TERM_EXIT Terminate;

void MQENTRY Terminate (  PMQAXP   pExitParms
                         , PMQAXC   pExitContext
                         , PMQLONG  pCompCode
                         , PMQLONG  pReason
                       ){

	IAE_ExitUserArea ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
	IAE_ExitUserArea  * pExitUserArea  = *ppExitUserArea;

	*ppExitUserArea = 0;

	//Bug in WMQ thread clone() ?

	//if(pExitUserArea){
	//	DBG(pExitUserArea,"Freeing memory. %lx %lx \n",pExitUserArea,ppExitUserArea);
	//	free(pExitUserArea);
	//}

}
/*********************************************************************/

typedef struct {
  MQLONG     ExitReason;
  MQLONG     Function;
  PMQFUNC    pEntryPoint;
  PMQXEPO    pExitOpts;
}IAE_ExitEntry;

#define IAE_NUM_EXITS 6

IAE_ExitEntry IAE_TabExits[IAE_NUM_EXITS] = {

   { MQXR_BEFORE, MQXF_OPEN, (PMQFUNC) OpenBefore, NULL },

   { MQXR_AFTER,  MQXF_GET,       (PMQFUNC) GetAfter,        NULL },
   { MQXR_AFTER,  MQXF_CALLBACK,  (PMQFUNC) CallbackAfter,   NULL },
   { MQXR_AFTER,  MQXF_PUT,       (PMQFUNC) PutAfter,        NULL },
   { MQXR_AFTER,  MQXF_PUT1,      (PMQFUNC) Put1After,       NULL },

   { MQXR_CONNECTION, MQXF_TERM, (PMQFUNC) Terminate, NULL }

};

/*********************************************************************/
/* Initialisation function                                           */
/*********************************************************************/

MQ_INIT_EXIT EntryPoint;

void MQENTRY EntryPoint(PMQAXP pExitParms,
						PMQAXC pExitContext,
						PMQLONG pCompCode,
						PMQLONG pReason){

	IAE_ExitUserArea ** ppExitUserArea = (void*) &pExitParms->ExitUserArea;
	IAE_ExitUserArea  * pExitUserArea  = NULL;

	pExitParms->ExitResponse = MQXCC_OK;

	char sParameters[sizeof(pExitParms->ExitData)+1];

	  /*******************************************************************/
	  /* Make sure that the Hconfig supplied contains the interface      */
	  /* entry points.                                                   */
	  /*******************************************************************/
	  if (memcmp(pExitParms->Hconfig->StrucId, MQIEP_STRUC_ID, 4)){
	    pExitParms->ExitResponse = MQXCC_FAILED;
	    goto out;
	  }

	  /*******************************************************************/
	  /* Malloc storage for the ExitUserArea                             */
	  /*******************************************************************/

		memcpy(sParameters,pExitParms->ExitData,sizeof(pExitParms->ExitData));
		sParameters[sizeof(pExitParms->ExitData)]=0;

		pExitUserArea = IAE_CreateExitData(sParameters);

		if (pExitUserArea){
			*ppExitUserArea = pExitUserArea;
		}else {
			pExitParms->ExitResponse = MQXCC_FAILED;
			goto out;
		}

	/***************************************************************/
	/* Register the MQGET entrypoints                              */
	/***************************************************************/

		int iIdx;

		for (iIdx = 0; iIdx < IAE_NUM_EXITS; iIdx++) {

			IAE_ExitEntry *pEntry = IAE_TabExits + iIdx;

			DBG(pExitUserArea,"Registration[%d]: function: %s \n",iIdx, IAE_FunctionId(pEntry->Function));

			pExitParms->Hconfig->MQXEP_Call(pExitParms->Hconfig,
										pEntry->ExitReason,
										pEntry->Function,
										pEntry->pEntryPoint,
										pEntry->pExitOpts,
										pCompCode,
										pReason);

			if (*pReason != MQRC_NONE) {
				pExitParms->ExitResponse = MQXCC_FAILED;
				goto out;
			}

		}

out:

	 if(pExitParms->ExitResponse != MQXCC_OK && *ppExitUserArea){
		 free(*ppExitUserArea);
		 *ppExitUserArea = NULL;
	 }

}


/********************************************************************/
#if MQAT_DEFAULT == MQAT_UNIX
void MQStart(){;}
#endif
