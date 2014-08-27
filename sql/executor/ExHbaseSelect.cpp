// **********************************************************************
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2013-2014 Hewlett-Packard Development Company, L.P.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// @@@ END COPYRIGHT @@@
// **********************************************************************

#include "Platform.h"

#include "ex_stdh.h"
#include "ComTdb.h"
#include "ex_tcb.h"
#include "ExHbaseAccess.h"
#include "ex_exe_stmt_globals.h"
#include "SQLTypeDefs.h"

#include "ExpHbaseInterface.h"

ExHbaseScanTaskTcb::ExHbaseScanTaskTcb(
				       ExHbaseAccessSelectTcb * tcb)
  :  ExHbaseTaskTcb(tcb)
  , step_(NOT_STARTED)
{
}

void ExHbaseScanTaskTcb::init() 
{
  step_ = NOT_STARTED;
}

ExWorkProcRetcode ExHbaseScanTaskTcb::work(short &rc)
{
  Lng32 retcode = 0;
  rc = 0;

  while (1)
    {
      ex_queue_entry *pentry_down = tcb_->qparent_.down->getHeadEntry();
      
      switch (step_)
	{
	case NOT_STARTED:
	  {
	    step_ = SCAN_OPEN;
	  }
	  break;

	case SCAN_OPEN:
	  {
	    tcb_->table_.val = tcb_->hbaseAccessTdb().getTableName();
	    tcb_->table_.len = strlen(tcb_->hbaseAccessTdb().getTableName());

	    retcode = tcb_->ehi_->scanOpen(tcb_->table_, 
					   tcb_->beginRowId_, tcb_->endRowId_,
					   tcb_->columns_, -1,
					   tcb_->hbaseAccessTdb().readUncommittedScan(),
					   tcb_->hbaseAccessTdb().getHbasePerfAttributes()->cacheBlocks(),
					   tcb_->hbaseAccessTdb().getHbasePerfAttributes()->numCacheRows(),
					   NULL, NULL, NULL);
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanOpen"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = SCAN_FETCH;
	  }
	  break;

	case SCAN_FETCH:
	  {
	    retcode = tcb_->ehi_->scanFetch(tcb_->rowId_, tcb_->colFamName_, 
					    tcb_->colName_, tcb_->colVal_, tcb_->colTS_);
	    if (retcode == HBASE_ACCESS_EOD)
	      {
		step_ = SCAN_CLOSE;
		break;
	      }

	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanFetch"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = CREATE_ROW;
	  }
	  break;

	case CREATE_ROW:
	  {
	    rc = tcb_->createColumnwiseRow();
	    if (rc == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incAccessedRows();

	    step_ = APPLY_PRED;
	  }
	  break;

	  case APPLY_PRED:
	  {
	    rc = tcb_->applyPred(tcb_->scanExpr());
	    if (rc == 1)
	      step_ = RETURN_ROW;
	    else if (rc == -1)
	      step_ = HANDLE_ERROR;
	    else
	      step_ = SCAN_FETCH;
	  }
	  break;

	case RETURN_ROW:
	  {
	    if (tcb_->moveRowToUpQueue(tcb_->convertRow_, tcb_->hbaseAccessTdb().convertRowLen(), 
				 &rc, FALSE))
	      return 1;
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incUsedRows();

	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == tcb_->matches_))
	      {
		step_ = SCAN_CLOSE;
		break;
	      }

	    step_ = SCAN_FETCH;
	  }
	  break;

	case SCAN_CLOSE:
	  {
	    retcode = tcb_->ehi_->scanClose();
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanClose"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	  {
	    step_ = NOT_STARTED;
	    return -1;
	  }
	  break;

	case DONE:
	  {
	    step_ = NOT_STARTED;
	    return 0;
	  }
	  break;

	}// switch

    } // while
}

ExHbaseScanRowwiseTaskTcb::ExHbaseScanRowwiseTaskTcb(ExHbaseAccessSelectTcb * tcb)
  :  ExHbaseTaskTcb(tcb)
  , step_(NOT_STARTED)
{
}

void ExHbaseScanRowwiseTaskTcb::init() 
{
  step_ = NOT_STARTED;
}

ExWorkProcRetcode ExHbaseScanRowwiseTaskTcb::work(short &rc)
{
  Lng32 retcode = 0;
  rc = 0;

  while (1)
    {
      ex_queue_entry *pentry_down = tcb_->qparent_.down->getHeadEntry();
      
      switch (step_)
	{
	case NOT_STARTED:
	  {
	    step_ = SCAN_OPEN;
	  }
	  break;

	case SCAN_OPEN:
	  {
	    tcb_->table_.val = tcb_->hbaseAccessTdb().getTableName();
	    tcb_->table_.len = strlen(tcb_->hbaseAccessTdb().getTableName());

	    retcode = tcb_->ehi_->scanOpen(tcb_->table_, 
					   tcb_->beginRowId_, tcb_->endRowId_,
					   tcb_->columns_, -1,
					   tcb_->hbaseAccessTdb().readUncommittedScan(),
					   tcb_->hbaseAccessTdb().getHbasePerfAttributes()->cacheBlocks(),
					   tcb_->hbaseAccessTdb().getHbasePerfAttributes()->numCacheRows(),
					   NULL, NULL, NULL);
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanOpen"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = SCAN_FETCH;

	    tcb_->isEOD_ = FALSE;
	  }
	  break;

	case SCAN_FETCH:
	  {
	    retcode = tcb_->ehi_->scanFetch(tcb_->rowId_, tcb_->colFamName_, 
					    tcb_->colName_, tcb_->colVal_,
					    tcb_->colTS_);

	    if (retcode == HBASE_ACCESS_EOD)
	      {
		tcb_->isEOD_ = TRUE;

		//		if ((! tcb_->prevRowId_.val) || (strlen(tcb_->rowwiseRow_) == 0))
		if ((! tcb_->prevRowId_.val) || (tcb_->rowwiseRowLen_ == 0))
		  step_ = DONE;
		else
		  step_ = CREATE_ROW;
		break;
	      }

	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanFetch"))
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    if (!tcb_->prevRowId_.val)
	      {
		tcb_->setupPrevRowId();
	      }
	    else if (memcmp(tcb_->prevRowId_.val, tcb_->rowId_.val, tcb_->rowId_.len) == 0)
	      {
		//		str_sprintf(&tcb_->rowwiseRow_[strlen(tcb_->rowwiseRow_)], ", ");
	      }
	    else
	      {
		step_ = CREATE_ROW;
		break;
	      }

	    step_ = APPEND_ROW;
	  }
	  break;

	case APPEND_ROW:
	  {
	    // values are stored in the following format:
	    //  colNameLen(2 bytes)
	    //  colName for colNameLen bytes
	    //  colValueLen(4 bytes)
	    //  colValue for colValueLen bytes.
	    //
	    // Attribute values are not necessarily null-terminated. 
	    // Cannot use string functions.
	    //
	    char* pos = tcb_->rowwiseRow_ + tcb_->rowwiseRowLen_;

	    short colNameLen = tcb_->colName_.len;
	    memcpy(pos, (char*)&colNameLen, sizeof(short));
	    pos += sizeof(short);

	    memcpy(pos, tcb_->colName_.val, colNameLen);
	    pos += colNameLen;

	    Lng32 colValueLen = tcb_->colVal_.len;
	    memcpy(pos, (char*)&colValueLen, sizeof(Lng32));
	    pos += sizeof(Lng32);

	    memcpy(pos, tcb_->colVal_.val, colValueLen);
	    pos += colValueLen;

	    tcb_->rowwiseRowLen_ += sizeof(short) + colNameLen + sizeof(Lng32) + colValueLen;
	    step_ = SCAN_FETCH;
	  }
	  break;

	case CREATE_ROW:
	  {
	    rc = tcb_->createRowwiseRow();
	    if (rc == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incAccessedRows();

	    step_ = APPLY_PRED;
	  }
	  break;

	  case APPLY_PRED:
	  {
	    rc = tcb_->applyPred(tcb_->scanExpr());

	    if (rc == 1)
	      {
		step_ = RETURN_ROW;
		break;
	      }
	    else if (rc == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }
	    else if (tcb_->isEOD_)
	      step_ = SCAN_CLOSE;
	    else
	      step_ = APPEND_ROW;

	    tcb_->setupPrevRowId();
	  }
	  break;

	case RETURN_ROW:
	  {
	    rc = 0;
	    if (tcb_->moveRowToUpQueue(tcb_->convertRow_, tcb_->hbaseAccessTdb().convertRowLen(), 
				 &rc, FALSE))
	      return 1;

	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incUsedRows();

	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == tcb_->matches_))
	      {
		step_ = SCAN_CLOSE;
		break;
	      }

	    tcb_->setupPrevRowId();

	    if (tcb_->isEOD_)
	      step_ = SCAN_CLOSE;
	    else
	      step_ = APPEND_ROW;
	  }
	  break;

	case SCAN_CLOSE:
	  {
	    retcode = tcb_->ehi_->scanClose();
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanClose"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	  {
	    step_ = NOT_STARTED;
	    return -1;
	  }
	  break;

	case DONE:
	  {
	    step_ = NOT_STARTED;
	    return 0;
	  }
	  break;

	}// switch

    } // while

  return 0; // WORK_OK
}

ExHbaseScanSQTaskTcb::ExHbaseScanSQTaskTcb(
				       ExHbaseAccessSelectTcb * tcb)
  :  ExHbaseTaskTcb(tcb)
  , step_(NOT_STARTED)
{
}

void ExHbaseScanSQTaskTcb::init() 
{
  step_ = NOT_STARTED;
}

ExWorkProcRetcode ExHbaseScanSQTaskTcb::work(short &rc)
{
  Lng32 retcode = 0;
  rc = 0;

  while (1)
    {
      ex_queue_entry *pentry_down = tcb_->qparent_.down->getHeadEntry();
      
      switch (step_)
	{
	case NOT_STARTED:
	  {
	    step_ = SCAN_OPEN;
	  }
	  break;

	case SCAN_OPEN:
	  {
	    tcb_->table_.val = tcb_->hbaseAccessTdb().getTableName();
	    tcb_->table_.len = strlen(tcb_->hbaseAccessTdb().getTableName());

	    if (tcb_->setupHbaseFilterPreds())
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    retcode = tcb_->ehi_->scanOpen(tcb_->table_, 
					   tcb_->beginRowId_, tcb_->endRowId_,
					   tcb_->columns_, -1,
					   tcb_->hbaseAccessTdb().readUncommittedScan(),
					   tcb_->hbaseAccessTdb().getHbasePerfAttributes()->cacheBlocks(),
					   tcb_->hbaseAccessTdb().getHbasePerfAttributes()->numCacheRows(),
					   (tcb_->hbaseFilterColumns_.size() > 0 ?
					    &tcb_->hbaseFilterColumns_ : NULL),
					   (tcb_->hbaseFilterOps_.size() > 0 ?
					    &tcb_->hbaseFilterOps_ : NULL),
					   (tcb_->hbaseFilterValues_.size() > 0 ?
					    &tcb_->hbaseFilterValues_ : NULL),
					    tcb_->getSamplePercentage());

	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanOpen"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = SCAN_FETCH_NEXT_ROW;
	  }
	  break;

	case SCAN_FETCH_NEXT_ROW:
	  {
	    retcode = tcb_->ehi_->nextRow();
	    if (retcode == HBASE_ACCESS_EOD || retcode == HBASE_ACCESS_EOR)
	      {
		step_ = SCAN_CLOSE;
		break;
	      }
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::nextRow"))
               step_ = HANDLE_ERROR; 
            else
	       step_ = CREATE_ROW;
	  }
	  break;
	case CREATE_ROW:
	  {
	    retcode = tcb_->createSQRowDirect();
	    if (retcode == HBASE_ACCESS_NO_ROW)
	    {
	        step_ = SCAN_FETCH_NEXT_ROW;
	        break;
	    }
	    if (retcode < 0)
	    {
	        rc = (short)retcode;
	        tcb_->setupError(rc, "createSQRowDirect");
		step_ = HANDLE_ERROR;
		break;
	    }
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incAccessedRows();

	    step_ = APPLY_PRED;
	  }
	  break;

	  case APPLY_PRED:
	  {
	    rc = tcb_->applyPred(tcb_->scanExpr());
	    if (rc == 1)
	      step_ = RETURN_ROW;
	    else if (rc == -1)
	      step_ = HANDLE_ERROR;
	    else
              step_ = SCAN_FETCH_NEXT_ROW;
	  }
	  break;

	case RETURN_ROW:
	  {
	    if (tcb_->moveRowToUpQueue(tcb_->convertRow_, tcb_->hbaseAccessTdb().convertRowLen(), 
				 &rc, FALSE))
	      return 1;
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incUsedRows();
	    
	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == tcb_->matches_))
	      {
		step_ = SCAN_CLOSE;
		break;
	      }

            step_ = SCAN_FETCH_NEXT_ROW;
	  }
	  break;

	case SCAN_CLOSE:
	  {
	    retcode = tcb_->ehi_->scanClose();
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::scanClose"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	  {
	    step_ = NOT_STARTED;
	    return -1;
	  }
	  break;

	case DONE:
	  {
	    step_ = NOT_STARTED;
	    return 0;
	  }
	  break;

	}// switch

    } // while
}

Lng32 ExHbaseScanSQTaskTcb::getProbeResult(char* &keyData)
{
  Lng32 retcode = 0;
  Lng32 rc = 0;

  retcode = tcb_->ehi_->scanOpen(tcb_->table_, 
				 tcb_->beginRowId_, tcb_->endRowId_,
				 tcb_->columns_, -1,
				 tcb_->hbaseAccessTdb().readUncommittedScan(),
				 tcb_->hbaseAccessTdb().getHbasePerfAttributes()->cacheBlocks(),
				 tcb_->hbaseAccessTdb().getHbasePerfAttributes()->numCacheRows(),
				 NULL, NULL, NULL);
  if (tcb_->setupError(retcode, "ExpHbaseInterface::scanOpen"))
    {
      rc = -1;
      goto label_return;
    }
  
  retcode = tcb_->ehi_->nextRow();
  if (retcode == HBASE_ACCESS_EOD || retcode == HBASE_ACCESS_EOR)
    {
      rc = 1; // no row found
      goto label_return;
    }
  if (tcb_->setupError(retcode, "ExpHbaseInterface::nextRow"))
    {
      rc = -1;
      goto label_return;
    }
  retcode = tcb_->createSQRowDirect();
  if (retcode == HBASE_ACCESS_NO_ROW)
  {
     rc = 1;
     goto label_return;
  }
  if (retcode < 0)
  {
      rc = retcode;
      tcb_->setupError(rc, "createSQRowDirect");
      rc = -1;
      goto label_return;
  }

  // extract the key from the fetched row, encode it and pass it back to mdam
  if (tcb_->evalEncodedKeyExpr() == -1)
    {
      rc = -1;
      goto label_return;
    }
  
 label_return:
  retcode = tcb_->ehi_->scanClose();
  if (tcb_->setupError(retcode, "ExpHbaseInterface::scanClose"))
    {
      rc = -1;
    }

  keyData = tcb_->encodedKeyRow_;

  return rc;
}

ExHbaseGetTaskTcb::ExHbaseGetTaskTcb(
				     ExHbaseAccessSelectTcb * tcb)
  :  ExHbaseTaskTcb(tcb)
  , step_(NOT_STARTED)
{
}

void ExHbaseGetTaskTcb::init() 
{
  step_ = NOT_STARTED;
}

ExWorkProcRetcode ExHbaseGetTaskTcb::work(short &rc)
{
  Lng32 retcode = 0;
  rc = 0;

  while (1)
    {
      ex_queue_entry *pentry_down = tcb_->qparent_.down->getHeadEntry();
      
      switch (step_)
	{
	case NOT_STARTED:
	  {
	    step_ = GET_OPEN;
	  }
	  break;

	case GET_OPEN:
	  {
	    tcb_->table_.val = tcb_->hbaseAccessTdb().getTableName();
	    tcb_->table_.len = strlen(tcb_->hbaseAccessTdb().getTableName());

	    if (tcb_->evalRowIdExpr() == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    if (tcb_->rowIds_.size() == 1)
	      {
		retcode = tcb_->ehi_->getRowOpen(tcb_->table_, tcb_->rowIds_[0],
					     tcb_->columns_, -1, FALSE);
		if (tcb_->setupError(retcode, "ExpHbaseInterface::getRowOpen"))
		  step_ = HANDLE_ERROR;
		else
		  step_ = GET_FETCH;
	      }
	    else
	      {
		retcode = tcb_->ehi_->getRowsOpen(tcb_->table_, tcb_->rowIds_,
					     tcb_->columns_, -1, FALSE);
		if (tcb_->setupError(retcode, "ExpHbaseInterface::getRowsOpen"))
		  step_ = HANDLE_ERROR;
		else
		  step_ = GET_FETCH;
	      }
	      
	  }
	  break;

	case GET_FETCH:
	  {
	    retcode = tcb_->ehi_->getFetch(tcb_->rowId_, tcb_->colFamName_, 
					   tcb_->colName_, tcb_->colVal_, tcb_->colTS_);
	    if (retcode == HBASE_ACCESS_EOD)
	      {
		step_ = GET_CLOSE;
		break;
	      }

	    if (tcb_->setupError(retcode, "ExpHbaseInterface::getFetch"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = CREATE_ROW;
	  }
	  break;

	case CREATE_ROW:
	  {
	    rc = tcb_->createColumnwiseRow();
	    if (rc == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incAccessedRows();

	    step_ = APPLY_PRED;
	  }
	  break;

	case APPLY_PRED:
	  {
	    rc = tcb_->applyPred(tcb_->scanExpr());

	    if (rc == 1)
	      step_ = RETURN_ROW;
	    else if (rc == -1)
	      step_ = HANDLE_ERROR;
	    else
	      step_ = GET_FETCH;
	  }
	  break;

	case RETURN_ROW:
	  {
	    rc = 0;
	    if (tcb_->moveRowToUpQueue(tcb_->convertRow_, tcb_->hbaseAccessTdb().convertRowLen(), 
				       &rc, FALSE))
	      return 1;

	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incUsedRows();

	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == tcb_->matches_))
	      {
		step_ = GET_CLOSE;
		break;
	      }

	    step_ = GET_FETCH;
	  }
	  break;

	case GET_CLOSE:
	  {
	    retcode = tcb_->ehi_->getClose();
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::getClose"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	  {
	    step_ = NOT_STARTED;
	    return -1;
	  }
	  break;
	    
	case DONE:
	  {
	    step_ = NOT_STARTED;
	    return 0;
	  }
	  break;

	}// switch

    } // while
}

ExHbaseGetRowwiseTaskTcb::ExHbaseGetRowwiseTaskTcb(
				       ExHbaseAccessSelectTcb * tcb)
  :  ExHbaseTaskTcb(tcb)
  , step_(NOT_STARTED)
{
}

void ExHbaseGetRowwiseTaskTcb::init() 
{
  step_ = NOT_STARTED;
}

ExWorkProcRetcode ExHbaseGetRowwiseTaskTcb::work(short &rc)
{
  Lng32 retcode = 0;
  rc = 0;

  while (1)
    {
      ex_queue_entry *pentry_down = tcb_->qparent_.down->getHeadEntry();
      
      switch (step_)
	{
	case NOT_STARTED:
	  {
	    step_ = GET_OPEN;
	  }
	  break;

	case GET_OPEN:
	  {
	    tcb_->table_.val = tcb_->hbaseAccessTdb().getTableName();
	    tcb_->table_.len = strlen(tcb_->hbaseAccessTdb().getTableName());

	    if (tcb_->evalRowIdExpr() == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    if (tcb_->rowIds_.size() == 1)
	      {
		retcode = tcb_->ehi_->getRowOpen(tcb_->table_, tcb_->rowIds_[0],
					     tcb_->columns_, -1, FALSE);
		if (tcb_->setupError(retcode, "ExpHbaseInterface::getRowOpen"))
		  step_ = HANDLE_ERROR;
		else
		  step_ = GET_FETCH;
	      }
	    else
	      {
		retcode = tcb_->ehi_->getRowsOpen(tcb_->table_, tcb_->rowIds_,
					     tcb_->columns_, -1, FALSE);
		if (tcb_->setupError(retcode, "ExpHbaseInterface::getRowsOpen"))
		  step_ = HANDLE_ERROR;
		else
		  step_ = GET_FETCH;
	      }

	  }
	  break;

	case GET_FETCH:
	  {
	    retcode = tcb_->ehi_->getFetch(tcb_->rowId_, tcb_->colFamName_, 
					   tcb_->colName_, tcb_->colVal_, tcb_->colTS_);
	    if (retcode == HBASE_ACCESS_EOD)
	      {
		if (! tcb_->prevRowId_.val)
		  step_ = DONE;
		else
		  step_ = CREATE_ROW;
		break;
	      }

	    if (!tcb_->prevRowId_.val)
	      {
		tcb_->setupPrevRowId();
	      }
	    else
	      {
		//		str_sprintf(&tcb_->rowwiseRow_[strlen(tcb_->rowwiseRow_)], ", ");
	      }
	      
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::getFetch"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = APPEND_ROW;
	  }
	  break;

	case APPEND_ROW:
	  {
	    // values are stored in the following format:
	    //  colNameLen(2 bytes)
	    //  colName for colNameLen bytes
	    //  colValueLen(4 bytes)
	    //  colValue for colValueLen bytes.
	    //
	    // Attribute values are not necessarily null-terminated. 
	    // Cannot use string functions.
	    //
	    char* pos = tcb_->rowwiseRow_ + tcb_->rowwiseRowLen_;

	    short colNameLen = tcb_->colName_.len;
	    memcpy(pos, (char*)&colNameLen, sizeof(short));
	    pos += sizeof(short);

	    memcpy(pos, tcb_->colName_.val, colNameLen);
	    pos += colNameLen;

	    Lng32 colValueLen = tcb_->colVal_.len;
	    memcpy(pos, (char*)&colValueLen, sizeof(Lng32));
	    pos += sizeof(Lng32);

	    memcpy(pos, tcb_->colVal_.val, colValueLen);
	    pos += colValueLen;

	    tcb_->rowwiseRowLen_ += sizeof(short) + colNameLen + sizeof(Lng32) + colValueLen;
	    step_ = GET_FETCH;
	  }
	  break;

	case CREATE_ROW:
	  {
	    rc = tcb_->createRowwiseRow();
	    if (rc == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incAccessedRows();

	    step_ = APPLY_PRED;
	  }
	  break;

	case APPLY_PRED:
	  {
	    rc = tcb_->applyPred(tcb_->scanExpr());

	    if (rc == 1)
	      step_ = RETURN_ROW;
	    else if (rc == -1)
	      step_ = HANDLE_ERROR;
	    else
	      step_ = GET_CLOSE;
	  }
	  break;

	case RETURN_ROW:
	  {
	    rc = 0;
	    if (tcb_->moveRowToUpQueue(tcb_->convertRow_, tcb_->hbaseAccessTdb().convertRowLen(), 
				 &rc, FALSE))
	      return 1;
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incUsedRows();

	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == tcb_->matches_))
	      {
		step_ = GET_CLOSE;
		break;
	      }

	    step_ = GET_CLOSE;
	  }
	  break;

	case GET_CLOSE:
	  {
	    retcode = tcb_->ehi_->getClose();
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::getClose"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	  {
	    step_ = NOT_STARTED;
	    return -1;
	  }
	  break;
	    
	case DONE:
	  {
	    step_ = NOT_STARTED;
	    return 0;
	  }
	  break;

	}// switch

    } // while
}

ExHbaseGetSQTaskTcb::ExHbaseGetSQTaskTcb(
					 //				     ExHbaseAccessSelectTcb * tcb)
					 ExHbaseAccessTcb * tcb)
  :  ExHbaseTaskTcb(tcb)
  , step_(NOT_STARTED)
{
}

void ExHbaseGetSQTaskTcb::init() 
{
  step_ = NOT_STARTED;
}

ExWorkProcRetcode ExHbaseGetSQTaskTcb::work(short &rc)
{
  Lng32 retcode = 0;
  rc = 0;

  while (1)
    {
      ex_queue_entry *pentry_down = tcb_->qparent_.down->getHeadEntry();
      
      switch (step_)
	{
	case NOT_STARTED:
	  {
	    step_ = GET_OPEN;
	  }
	  break;

	case GET_OPEN:
	  {
	    tcb_->table_.val = tcb_->hbaseAccessTdb().getTableName();
	    tcb_->table_.len = strlen(tcb_->hbaseAccessTdb().getTableName());

	    if (tcb_->rowIds_.size() == 1)
	      {
		retcode = tcb_->ehi_->getRowOpen(tcb_->table_, tcb_->rowIds_[0],
					     tcb_->columns_, -1, TRUE);
		if (tcb_->setupError(retcode, "ExpHbaseInterface::getRowOpen"))
		  step_ = HANDLE_ERROR;
		else
		  step_ = GET_FETCH;
	      }
	    else
	      {
		retcode = tcb_->ehi_->getRowsOpen(tcb_->table_, tcb_->rowIds_,
					     tcb_->columns_, -1, TRUE);
		if (tcb_->setupError(retcode, "ExpHbaseInterface::getRowsOpen"))
		  step_ = HANDLE_ERROR;
		else
		  step_ = GET_FETCH;
	      }
	      
	  }
	  break;

	case GET_FETCH:
	  {
	    retcode = tcb_->ehi_->nextRow();
            // EOD is end of data, EOR is end of result set. 
            // for single get, EOD or EOR indicates DONE
            // for multi get, only EOR indicates DONE
	    if ( (retcode == HBASE_ACCESS_EOR) ||
                 ( (retcode == HBASE_ACCESS_EOD) &&
                   (tcb_->rowIds_.size() == 1) ) )
	      {
		step_ = GET_CLOSE;
		break;
	      }
            
            // for multi get, do FETCH if retcode is EOD
            if ( (retcode == HBASE_ACCESS_EOD) && 
                 (tcb_->rowIds_.size() > 1) )
            {
                step_ = GET_FETCH;
                break;
            }
 
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::nextRow"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = CREATE_ROW;
	  }
	  break;

	case CREATE_ROW:
	  {
	    retcode = tcb_->createSQRowDirect();
	    if (retcode == HBASE_ACCESS_NO_ROW)
	    {
	       step_ = GET_FETCH;
               break;
            }
	    if (retcode != HBASE_ACCESS_SUCCESS)
	    {
		step_ = HANDLE_ERROR;
		break;
	    }
	    
	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incAccessedRows();

	    step_ = APPLY_PRED;
	  }
	  break;

	case APPLY_PRED:
	  {
	    rc = tcb_->applyPred(tcb_->scanExpr());

	    if (rc == 1)
	      step_ = RETURN_ROW;
	    else if (rc == -1)
	      step_ = HANDLE_ERROR;
	    else
	      step_ = GET_FETCH;
	  }
	  break;

	case RETURN_ROW:
	  {
	    rc = 0;
	    if (tcb_->moveRowToUpQueue(tcb_->convertRow_, tcb_->hbaseAccessTdb().convertRowLen(), 
				       &rc, FALSE))
	      return 1;

	    if (tcb_->getHbaseAccessStats())
	      tcb_->getHbaseAccessStats()->incUsedRows();

	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == tcb_->matches_))
	      {
		step_ = GET_CLOSE;
		break;
	      }

	    step_ = GET_FETCH;
	  }
	  break;

	case GET_CLOSE:
	  {
	    retcode = tcb_->ehi_->getClose();
	    if (tcb_->setupError(retcode, "ExpHbaseInterface::getClose"))
	      step_ = HANDLE_ERROR;
	    else
	      step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	  {
	    step_ = NOT_STARTED;
	    return -1;
	  }
	  break;
	    
	case DONE:
	  {
	    step_ = NOT_STARTED;
	    return 0;
	  }
	  break;

	}// switch

    } // while
}

ExHbaseAccessSelectTcb::ExHbaseAccessSelectTcb(
          const ExHbaseAccessTdb &hbaseAccessTdb, 
          ex_globals * glob ) :
  ExHbaseAccessTcb(hbaseAccessTdb, glob),
  step_(NOT_STARTED)
{
  scanRowwiseTaskTcb_ = NULL;
  scanTaskTcb_ = NULL;
  getRowwiseTaskTcb_ = NULL;
  getTaskTcb_ = NULL;
  scanSQTaskTcb_ = NULL;
  getSQTaskTcb_ = NULL;

  ExHbaseAccessTdb &hbaseTdb = (ExHbaseAccessTdb&)hbaseAccessTdb;
  samplePercentage_ = hbaseTdb.samplingRate_;

  if ((hbaseTdb.listOfScanRows()) ||
      ((hbaseTdb.keySubsetGen()) &&
       (NOT hbaseTdb.uniqueKeyInfo())) ||
      (hbaseTdb.keyMDAMGen()))
    {
      if (hbaseTdb.sqHbaseTable())
	scanSQTaskTcb_ = 
	  new(getGlobals()->getDefaultHeap()) ExHbaseScanSQTaskTcb(this);
      else if (hbaseTdb.rowwiseFormat())
	scanRowwiseTaskTcb_ = 
	  new(getGlobals()->getDefaultHeap()) ExHbaseScanRowwiseTaskTcb(this);
      else
	scanTaskTcb_ = 
	  new(getGlobals()->getDefaultHeap()) ExHbaseScanTaskTcb(this);
    }

  if ((hbaseTdb.listOfGetRows()) ||
      ((hbaseTdb.keySubsetGen()) &&
       (hbaseTdb.uniqueKeyInfo())) ||
      (hbaseTdb.keyMDAMGen()))
    {
      if (hbaseTdb.sqHbaseTable())
	getSQTaskTcb_ = 
	  new(getGlobals()->getDefaultHeap()) ExHbaseGetSQTaskTcb(this);
      else if (hbaseTdb.rowwiseFormat())
	getRowwiseTaskTcb_ = 
	  new(getGlobals()->getDefaultHeap()) ExHbaseGetRowwiseTaskTcb(this);
      else
	getTaskTcb_ = 
	  new(getGlobals()->getDefaultHeap()) ExHbaseGetTaskTcb(this);
     }

  if (hbaseTdb.sqHbaseTable())
    {
      scanTask_ = scanSQTaskTcb_;
      getTask_   = getSQTaskTcb_;
    }
  else if (hbaseTdb.rowwiseFormat())
    {
      scanTask_ = scanRowwiseTaskTcb_;
      getTask_   = getRowwiseTaskTcb_;
    }
  else
    {
      scanTask_ = scanTaskTcb_;
      getTask_   = getTaskTcb_;
    }
}

ExWorkProcRetcode ExHbaseAccessSelectTcb::work()
{
  Lng32 retcode = 0;
  short rc = 0;

  while (!qparent_.down->isEmpty())
    {
      ex_queue_entry *pentry_down = qparent_.down->getHeadEntry();
      if ((pentry_down->downState.request == ex_queue::GET_NOMORE) &&
	  (step_ != DONE))
	{
	  step_ = SELECT_CLOSE_NO_ERROR; //DONE;
	}

      switch (step_)
	{
	case NOT_STARTED:
	  {
	    matches_ = 0;

	    ex_assert(getHbaseAccessStats(), "hbase stats cannot be null");
	    
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->init();

	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == matches_))
	      {
		step_ = DONE;
		break;
	      }

	    step_ = SELECT_INIT;
	  }
	  break;

	case SELECT_INIT:
	  {
	    retcode = ehi_->init();
	    if (setupError(retcode, "ExpHbaseInterface::init"))
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    if (hbaseAccessTdb().listOfScanRows())
	      hbaseAccessTdb().listOfScanRows()->position();

	    if (hbaseAccessTdb().listOfGetRows())
	      hbaseAccessTdb().listOfGetRows()->position();

	    if (scanTask_)
	      scanTask_->init();

	    if (getTask_)
	      getTask_->init();
	    
	    step_ = SETUP_SCAN;
	  }
	  break;

	case SETUP_SCAN:
	  {
	    if ((! scanTask_) || (! hbaseAccessTdb().listOfScanRows()))
	      {
		step_ = SETUP_GET;
		break;
	      }

	    hsr_ = 
	      (ComTdbHbaseAccess::HbaseScanRows*)hbaseAccessTdb().listOfScanRows()
	      ->getCurr();

	    retcode = setupSubsetRowIdsAndCols(hsr_);
	    if (retcode == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    step_ = PROCESS_SCAN;
	  }
	  break;

	case PROCESS_SCAN:
	case PROCESS_SCAN_KEY:
	  {
           if (getHbaseAccessStats())
	      getHbaseAccessStats()->getTimer().start();

	    rc = 0;
	    retcode = scanTask_->work(rc);

          if (getHbaseAccessStats())
	      getHbaseAccessStats()->getTimer().stop();
	  
	    if (retcode == 1)
	      return rc;
	    else if (retcode < 0)
	      step_ = HANDLE_ERROR;
	    else if (step_ == PROCESS_SCAN_KEY)
	      step_ = SETUP_GET_KEY;
	    else
	      step_ = NEXT_SCAN;
	  }
	  break;

	case NEXT_SCAN:
	  {
	    hbaseAccessTdb().listOfScanRows()->advance();

	    if (! hbaseAccessTdb().listOfScanRows()->atEnd())
	      {
		step_ = SETUP_SCAN;
		break;
	      }

	    step_ = SETUP_GET;
	  }
	  break;

	case SETUP_GET:
	  {
	    if ((! getTask_) || (!hbaseAccessTdb().listOfGetRows()))
	      {
		step_ = SETUP_SCAN_KEY;
		break;
	      }

	    hgr_ = 
	      (ComTdbHbaseAccess::HbaseGetRows*)hbaseAccessTdb().listOfGetRows()
	      ->getCurr();

	    retcode = setupUniqueRowIdsAndCols(hgr_);
	    if (retcode == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    step_ = PROCESS_GET;
	  }
	  break;

	case PROCESS_GET:
	case PROCESS_GET_KEY:
	  {
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->getTimer().start();
	    
	    rc = 0;
	    retcode = getTask_->work(rc);
	    
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->getTimer().stop();

	    if (retcode == 1)
	      return rc;
	    else if (retcode < 0)
	      step_ = HANDLE_ERROR;
	    else if (step_ == PROCESS_GET_KEY)
	      step_ = SELECT_CLOSE;
	    else
	      step_ = NEXT_GET;
	  }
	  break;

	case NEXT_GET:
	  {
	    hbaseAccessTdb().listOfGetRows()->advance();

	    if (! hbaseAccessTdb().listOfGetRows()->atEnd())
	      {
		step_ = SETUP_GET;
		break;
	      }

	    step_ = SETUP_SCAN_KEY;
	  }
	  break;

	case SETUP_SCAN_KEY:
	  {
	    if (! hbaseAccessTdb().keySubsetGen())
	      {
		step_ = SELECT_CLOSE;
		break;
	      }

	    if (hbaseAccessTdb().uniqueKeyInfo())
	      {
		step_ = SETUP_GET_KEY;
		break;
	      }
	    
	    retcode = setupSubsetKeysAndCols();
	    if (retcode == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    step_ = PROCESS_SCAN_KEY;
	  }
	  break;

	case SETUP_GET_KEY:
	  {
	    if ((! getTask_) ||
		((hbaseAccessTdb().keySubsetGen()) &&
		 (NOT hbaseAccessTdb().uniqueKeyInfo())))
	      {
		step_ = SELECT_CLOSE;
		break;
	      }

	    retcode = setupUniqueKeyAndCols(TRUE);
	    if (retcode == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    step_ = PROCESS_GET_KEY;
	  }
	  break;

	case SELECT_CLOSE:
	case SELECT_CLOSE_NO_ERROR:
	  {
	    retcode = ehi_->close();

	    if (step_ == SELECT_CLOSE)
	      {
		if (setupError(retcode, "ExpHbaseInterface::close"))
		  {
		    step_ = HANDLE_ERROR_NO_CLOSE;
		    break;
		  }
	      }

	    step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	case HANDLE_ERROR_NO_CLOSE:
	  {
	    if (handleError(rc))
	      return rc;

	    if (step_ == HANDLE_ERROR)
	      retcode = ehi_->close();

	    step_ = DONE;
	  }
	  break;

	case DONE:
	  {
	    if (handleDone(rc))
	      return rc;

	    if (scanTask_)
	      scanTask_->init();

	    if (getTask_)
	      getTask_->init();
	    
	    step_ = NOT_STARTED;
	  }
	  break;

	} // switch
    } // while
        
  return WORK_OK;
}

ExHbaseAccessMdamSelectTcb::ExHbaseAccessMdamSelectTcb(
          const ExHbaseAccessTdb &hbaseAccessTdb, 
          ex_globals * glob ) :
  ExHbaseAccessSelectTcb(hbaseAccessTdb, glob),
  step_(NOT_STARTED)
{
}

ExWorkProcRetcode ExHbaseAccessMdamSelectTcb::work()
{
  Lng32 retcode = 0;
  short rc = 0;

  while (!qparent_.down->isEmpty())
    {
      ex_queue_entry *pentry_down = qparent_.down->getHeadEntry();
      if ((pentry_down->downState.request == ex_queue::GET_NOMORE) &&
	  (step_ != DONE))
	{
	  step_ = SELECT_CLOSE_NO_ERROR;
	}

      switch (step_)
	{
	case NOT_STARTED:
	  {
	    matches_ = 0;
	    matchesBeforeFetch_ = 0;

	    ex_assert(getHbaseAccessStats(), "hbase stats cannot be null");
	    
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->init();

	    if ((pentry_down->downState.request == ex_queue::GET_N) &&
		(pentry_down->downState.requestValue == matches_))
	      {
		step_ = DONE;
		break;
	      }
	    
	    step_ = SELECT_INIT;
	  }
	  break;

	case SELECT_INIT:
	  {
	    retcode = ehi_->init();
	    if (setupError(retcode, "ExpHbaseInterface::init"))
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    table_.val = hbaseAccessTdb().getTableName();
	    table_.len = strlen(hbaseAccessTdb().getTableName());

	    step_ = INIT_NEXT_KEY_RANGE;
	  }
	  break;

	case INIT_NEXT_KEY_RANGE:
	  {
	    retcode = initNextKeyRange(pool_, pentry_down->getAtp());
	    if (retcode == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }
	    
	    setupListOfColNames(hbaseAccessTdb().listOfFetchedColNames(),
				columns_);

	    fetchRangeHadRows_ = TRUE;

	    step_ = GET_NEXT_KEY_RANGE;
	  }
	  break;

	case GET_NEXT_KEY_RANGE:
	  {
	    keyRangeEx::getNextKeyRangeReturnType 
	      keyRangeStatus = setupSubsetKeys(fetchRangeHadRows_);

	    fetchRangeHadRows_ = FALSE;

	    if (keyRangeStatus == keyRangeEx::EXPRESSION_ERROR)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    if (keyRangeStatus == keyRangeEx::NO_MORE_RANGES)
	      {
		step_ = SELECT_CLOSE;
		break;
	      }

	    if (keyRangeStatus == keyRangeEx::PROBE_RANGE)
	      step_ = PROCESS_PROBE_RANGE;
	    else if (keyRangeStatus == keyRangeEx::FETCH_RANGE)
	      {
		matchesBeforeFetch_ = matches_;
		step_ = PROCESS_FETCH_RANGE;
	      }
	    else
	      step_ = HANDLE_ERROR;
	  }
	  break;

	case PROCESS_PROBE_RANGE:
	  {
	    char * keyData = NULL;
	    retcode = scanSQTaskTcb_->getProbeResult(keyData);
	    if (retcode == -1)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    if (retcode == 1) // no rows found
	      {
		keyExeExpr()->reportProbeResult(0);
		step_ = GET_NEXT_KEY_RANGE;
		break;
	      }

	    // pass the key value to the mdam generator
	    keyExeExpr()->reportProbeResult(keyData);
	    step_ = GET_NEXT_KEY_RANGE;
	  }
	  break;

	case PROCESS_FETCH_RANGE:
	  {
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->getTimer().start();
	    
	    rc = 0;
	    retcode = scanSQTaskTcb_->work(rc);
	    
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->getTimer().stop();

	    if (retcode == 1)
	      return rc;
	    else if (retcode < 0)
	      step_ = HANDLE_ERROR;
	    else
	      {
		if ((pentry_down->downState.request == ex_queue::GET_N) &&
		    (pentry_down->downState.requestValue == matches_))
		  {
		    step_ = SELECT_CLOSE;
		    break;
		  }

		if (matches_ > matchesBeforeFetch_)
		  fetchRangeHadRows_ = TRUE;

		step_ = GET_NEXT_KEY_RANGE;
	      }
	  }
	  break;

	case SELECT_CLOSE:
	case SELECT_CLOSE_NO_ERROR:
	  {
	    retcode = ehi_->close();
	    if (step_ == SELECT_CLOSE)
	      {
		if (setupError(retcode, "ExpHbaseInterface::close"))
		  {
		    step_ = HANDLE_ERROR_NO_CLOSE;
		    break;
		  }
	      }

	    step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	case HANDLE_ERROR_NO_CLOSE:
	  {
	    if (handleError(rc))
	      return rc;

	    if (step_ == HANDLE_ERROR)
	      retcode = ehi_->close();

	    step_ = DONE;
	  }
	  break;

	case DONE:
	  {
	    if (handleDone(rc))
	      return rc;

	    step_ = NOT_STARTED;
	  }
	  break;

	} // switch
    } // while

  return WORK_OK;
}

ExHbaseCoProcAggrTcb::ExHbaseCoProcAggrTcb(
          const ComTdbHbaseCoProcAggr &hbaseAccessTdb, 
          ex_globals * glob ) :
  ExHbaseAccessTcb(hbaseAccessTdb, glob),
  step_(NOT_STARTED)
{
}

ExWorkProcRetcode ExHbaseCoProcAggrTcb::work()
{
  Lng32 retcode = 0;
  short rc = 0;

  while (!qparent_.down->isEmpty())
    {
      ex_queue_entry *pentry_down = qparent_.down->getHeadEntry();
      if ((pentry_down->downState.request == ex_queue::GET_NOMORE) &&
	  (step_ != DONE))
	{
	  //	  step_ = SELECT_CLOSE_NO_ERROR;
	}

      switch (step_)
	{
	case NOT_STARTED:
	  {
	    matches_ = 0;

	    ex_assert(getHbaseAccessStats(), "hbase stats cannot be null");
	    
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->init();

	    step_ = COPROC_INIT;
	  }
	  break;

	case COPROC_INIT:
	  {
	    retcode = ehi_->init();
	    if (setupError(retcode, "ExpHbaseInterface::init"))
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    table_.val = hbaseAccessTdb().getTableName();
	    table_.len = strlen(hbaseAccessTdb().getTableName());

	    hbaseAccessTdb().listOfFetchedColNames()->position();
	    hbaseAccessTdb().listOfAggrTypes()->position();

	    aggrIdx_ = 0;

	    step_ = COPROC_EVAL;
	  }
	  break;

	case COPROC_EVAL:
	  {
	    Lng32 aggrType = *(short*)hbaseAccessTdb().listOfAggrTypes()->getCurr();
	    char * col = (char*)hbaseAccessTdb().listOfFetchedColNames()->getCurr();

	    Text aggrVal;
	    Text colFam;
	    Text colName;
	    retcode = extractColFamilyAndName(col, colFam, colName);
	    if (retcode)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    retcode = ehi_->coProcAggr(table_, 
				       aggrType, 
				       "", // startRow
				       "", // stopRow
				       colFam, 
				       colName, 
				       FALSE, // cacheBlocks
				       100, //numCacheRows
				       aggrVal);
	    if (setupError(retcode, "ExpHbaseInterface::coProcAggr"))
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    ExpTupleDesc * convertTuppTD =
	      hbaseAccessTdb().workCriDesc_->getTupleDescriptor
	      (hbaseAccessTdb().convertTuppIndex_);

	    Attributes * attr = convertTuppTD->getAttr(aggrIdx_);
	    if (! attr)
	      {
		step_ = HANDLE_ERROR;
		break;
	      }

	    if (attr->getNullFlag())
	      {
		*(short*)&convertRow_[attr->getNullIndOffset()] = 0;
	      }

	    str_cpy_all(&convertRow_[attr->getOffset()], aggrVal.data(), aggrVal.length());

	    hbaseAccessTdb().listOfAggrTypes()->advance();
	    hbaseAccessTdb().listOfFetchedColNames()->advance();
	    aggrIdx_++;

	    if (hbaseAccessTdb().listOfAggrTypes()->atEnd())
	      {
		step_ = RETURN_ROW;
		break;
	      }
	  }
	  break;

	case RETURN_ROW:
	  {
	    short rc = 0;
	    if (moveRowToUpQueue(convertRow_, hbaseAccessTdb().convertRowLen(), 
				 &rc, FALSE))
	      return 1;
	    
	    if (getHbaseAccessStats())
	      getHbaseAccessStats()->incUsedRows();

	    step_ = DONE;
	  }
	  break;

	case HANDLE_ERROR:
	  {
	    if (handleError(rc))
	      return rc;

	    retcode = ehi_->close();

	    step_ = DONE;
	  }
	  break;

	case DONE:
	  {
	    if (handleDone(rc))
	      return rc;

	    step_ = NOT_STARTED;
	  }
	  break;

	} // switch
    } // while

  return WORK_OK;
}
