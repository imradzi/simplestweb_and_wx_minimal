#ifdef _WIN32
#include "winsock2.h"
#endif
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#ifdef __USE_MSSQL__
#include <atlbase.h>
#include <atlconv.h>
#include <sqloledb.h>
#include <strstream>
#endif

#include <iostream>
#include <string>
#include "sqlexception.h"
#include "wx/string.h"

bool SQLException::showWindow = false;
bool SQLException::showMessage = false;
bool SQLException::showLog = false;

//std::ofstream* SQLException::fOutPtr = NULL; //("Exception.Log");
#ifdef __USE_MSSQL__
wxString SQLException::GetStatusName( DBSTATUS st ) {
	switch (st) {
		case DBSTATUS_S_ISNULL: return "null";
		case DBSTATUS_S_OK: return "OK";
		case DBSTATUS_S_TRUNCATED: return "Truncated";
		case DBSTATUS_E_BADACCESSOR: return "Bad Accessor";
		case DBSTATUS_E_CANTCONVERTVALUE: return "Can't convert value";
		case DBSTATUS_E_CANTCREATE: return "Can't create";
		case DBSTATUS_E_DATAOVERFLOW: return "Data overflow";
		case DBSTATUS_E_SIGNMISMATCH: return "Sign mismatch";
		case DBSTATUS_E_UNAVAILABLE: return "No available";
	}
	return "??";
}

wxString SQLException::DumpErrorInfo(IUnknown* pObjectWithError, REFIID IID_InterfaceWithError, bool releaseObject) {

	wxString errMessage;

	CComPtr<IMalloc> pIMalloc;


	if (FAILED(CoGetMalloc(MEMCTX_TASK, &pIMalloc))) return "Error CoGetMalloc!";

    // Interfaces used in the example.
    IErrorInfo*             pIErrorInfoAll          = NULL;
    IErrorInfo*             pIErrorInfoRecord       = NULL;
    IErrorRecords*          pIErrorRecords          = NULL;
    ISupportErrorInfo*      pISupportErrorInfo      = NULL;
    ISQLErrorInfo*          pISQLErrorInfo          = NULL;

    // Number of error records.
    ULONG                   nRecs;
    ULONG                   nRec;

    // Basic error information from GetBasicErrorInfo.
    ERRORINFO               errorinfo;

    // IErrorInfo values.
    BSTR                    bstrDescription;
    BSTR                    bstrSource;

    // ISQLErrorInfo parameters.
    BSTR                    bstrSQLSTATE;
    LONG                    lNativeError;

    // ISQLServerErrorInfo parameter pointers.

	// Hard-code an American English locale for the example.
    DWORD                   MYLOCALEID = 0x0409;

    // Only ask for error information if the interface supports
    // it.
	if (FAILED(pObjectWithError->QueryInterface(IID_ISupportErrorInfo,(void**) &pISupportErrorInfo))) {
        if (releaseObject) pObjectWithError->Release();
//      if (showWindow) wxMessageBox("SupportErrorErrorInfo interface not supported"), "Error!"), wxICON_ERROR);
		else if (showMessage) std::cout << "SupportErrorErrorInfo interface not supported" << std::endl;
		return "SupportErrorErrorInfo interface not supported";
	}
	if (FAILED(pISupportErrorInfo->InterfaceSupportsErrorInfo(IID_InterfaceWithError))) {
        if (releaseObject) pObjectWithError->Release();
//      if (showWindow) wxMessageBox("InterfaceWithError interface not supported"),"Error!"), wxICON_ERROR);
		else if (showMessage) std::cout << "InterfaceWithError interface not supported" << std::endl;
        return "InterfaceWithError interface not supported";
	}

    // Do not test the return of GetErrorInfo. It can succeed and return
    // a NULL pointer in pIErrorInfoAll. Simply test the pointer.
    GetErrorInfo(0, &pIErrorInfoAll);

    if (pIErrorInfoAll != NULL) {
        // Test to see if it's a valid OLE DB IErrorInfo interface
        // exposing a list of records.
        if (SUCCEEDED(pIErrorInfoAll->QueryInterface(IID_IErrorRecords, (void**) &pIErrorRecords))) {
            pIErrorRecords->GetRecordCount(&nRecs);

            // Within each record, retrieve information from each
            // of the defined interfaces.
            for (nRec = 0; nRec < nRecs; nRec++) {
                // From IErrorRecords, get the HRESULT and a reference
                // to the ISQLErrorInfo interface.
                pIErrorRecords->GetBasicErrorInfo(nRec, &errorinfo);
                pIErrorRecords->GetCustomErrorObject(nRec, IID_ISQLErrorInfo, (IUnknown**) &pISQLErrorInfo);

                // Display the HRESULT, then use the ISQLErrorInfo.
				errMessage.Append(" HRESULT:");
				errMessage.Append(wxString::Format("%X", errorinfo.hrError));
                if (pISQLErrorInfo != NULL) {
                    pISQLErrorInfo->GetSQLInfo(&bstrSQLSTATE, &lNativeError);

					errMessage.Append(" SQLSTATE:");
					errMessage.Append(bstrSQLSTATE);
					errMessage.Append(" Native Error:");
					errMessage.Append(wxString::Format("%ld", lNativeError));
                    SysFreeString(bstrSQLSTATE);

                    // Get the ISQLServerErrorInfo interface from
                    // ISQLErrorInfo before releasing the reference.
/*---- mri
					pISQLErrorInfo->QueryInterface(IID_ISQLServerErrorInfo, (void**) &pISQLServerErrorInfo);
----- mri */
                    pISQLErrorInfo->Release();
				}

                // Test to ensure the reference is valid, then
                // get error information from ISQLServerErrorInfo.
/*--- mri
				if (pISQLServerErrorInfo != NULL) {
					pISQLServerErrorInfo->GetErrorInfo(&pSSErrorInfo, &pSSErrorStrings);

                    // ISQLServerErrorInfo::GetErrorInfo succeeds
                    // even when it has nothing to return. Test the
                    // pointers before using.
                    if (pSSErrorInfo) {
                        // Display the state and severity from the
                        // returned information. The error message comes
                        // from IErrorInfo::GetDescription.
						errMessage << "Error state:\t" << int(pSSErrorInfo->bState) << std::endl;
						errMessage << "Severity:\t" << int(pSSErrorInfo->bClass) << std::endl;
                        // IMalloc::Free needed to release references
                        // on returned values. For the example, assume
                        // the g_pIMalloc pointer is valid.
						pIMalloc->Free(pSSErrorStrings);
                        pIMalloc->Free(pSSErrorInfo);
					}
                    pISQLServerErrorInfo->Release();
				}
--- mri */

                if (SUCCEEDED(pIErrorRecords->GetErrorInfo(nRec, MYLOCALEID, &pIErrorInfoRecord))) {
                    // Get the source and description (error message)
                    // from the record's IErrorInfo.
                    pIErrorInfoRecord->GetSource(&bstrSource);
                    pIErrorInfoRecord->GetDescription(&bstrDescription);

                    if (bstrSource != NULL) {
						errMessage.Append(" Source:");
						errMessage.Append(bstrSource);
                        SysFreeString(bstrSource);
					}
                    if (bstrDescription != NULL) {
						errMessage.Append(" Error message:");
						errMessage.Append(bstrDescription);
                        SysFreeString(bstrDescription);
					}

                    pIErrorInfoRecord->Release();
				}
			}
            pIErrorRecords->Release();
		} else {
            // IErrorInfo is valid; get the source and
            // description to see what it is.
            pIErrorInfoAll->GetSource(&bstrSource);
            pIErrorInfoAll->GetDescription(&bstrDescription);
            if (bstrSource != NULL) {
				errMessage.Append("Source:");
				errMessage.Append(bstrSource);
                SysFreeString(bstrSource);
			}
            if (bstrDescription != NULL) {
				errMessage.Append(" Error message:");
				errMessage.Append(bstrDescription);
                SysFreeString(bstrDescription);
			}
		}
        pIErrorInfoAll->Release();
	} else {
		errMessage = "GetErrorInfo failed.";
	}

    pISupportErrorInfo->Release();
	if (releaseObject) pObjectWithError->Release();
	if (showLog) wxLogMessage(errMessage);
	else if (showMessage) std::cout << errMessage.mb_str() << std::endl;
    return errMessage;
}

wxString SQLException::ShowError(const wxString &/*title*/) {
	CComPtr<IErrorInfo> errInfo;

	GetErrorInfo(0, &errInfo);
	BSTR bstrDesc;
	BSTR bstrSource;
	wxString msg="Error message: []";
	if (errInfo != NULL) {
		errInfo->GetDescription( &bstrDesc );
		errInfo->GetSource( &bstrSource );
		msg="Error message: [";
		msg.Append(bstrDesc);
		msg.Append(" ");
		msg.Append(bstrSource);
		msg.Append("]");
	}
	if (showLog) wxLogMessage(msg);
	else if (showMessage) std::cout << msg.mb_str() << std::endl;
	return msg;
}
#endif
