

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Tue Jan 19 07:14:07 2038
 */
/* Compiler settings for Quero.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.01.0622 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __Quero_h__
#define __Quero_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IQueroBand_FWD_DEFINED__
#define __IQueroBand_FWD_DEFINED__
typedef interface IQueroBand IQueroBand;

#endif 	/* __IQueroBand_FWD_DEFINED__ */


#ifndef __IQueroFilter_FWD_DEFINED__
#define __IQueroFilter_FWD_DEFINED__
typedef interface IQueroFilter IQueroFilter;

#endif 	/* __IQueroFilter_FWD_DEFINED__ */


#ifndef __QueroBand_FWD_DEFINED__
#define __QueroBand_FWD_DEFINED__

#ifdef __cplusplus
typedef class QueroBand QueroBand;
#else
typedef struct QueroBand QueroBand;
#endif /* __cplusplus */

#endif 	/* __QueroBand_FWD_DEFINED__ */


#ifndef __QueroFilter_FWD_DEFINED__
#define __QueroFilter_FWD_DEFINED__

#ifdef __cplusplus
typedef class QueroFilter QueroFilter;
#else
typedef struct QueroFilter QueroFilter;
#endif /* __cplusplus */

#endif 	/* __QueroFilter_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IQueroBand_INTERFACE_DEFINED__
#define __IQueroBand_INTERFACE_DEFINED__

/* interface IQueroBand */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IQueroBand;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3DBF9F47-B2FD-4A08-AF1E-653F5551217F")
    IQueroBand : public IDispatch
    {
    public:
    };
    
    
#else 	/* C style interface */

    typedef struct IQueroBandVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IQueroBand * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IQueroBand * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IQueroBand * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IQueroBand * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IQueroBand * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IQueroBand * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IQueroBand * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } IQueroBandVtbl;

    interface IQueroBand
    {
        CONST_VTBL struct IQueroBandVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IQueroBand_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IQueroBand_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IQueroBand_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IQueroBand_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IQueroBand_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IQueroBand_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IQueroBand_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IQueroBand_INTERFACE_DEFINED__ */


#ifndef __IQueroFilter_INTERFACE_DEFINED__
#define __IQueroFilter_INTERFACE_DEFINED__

/* interface IQueroFilter */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IQueroFilter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("057CD07D-8F03-47C5-809B-DF846BCEBB68")
    IQueroFilter : public IDispatch
    {
    public:
    };
    
    
#else 	/* C style interface */

    typedef struct IQueroFilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IQueroFilter * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IQueroFilter * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IQueroFilter * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IQueroFilter * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IQueroFilter * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IQueroFilter * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IQueroFilter * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } IQueroFilterVtbl;

    interface IQueroFilter
    {
        CONST_VTBL struct IQueroFilterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IQueroFilter_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IQueroFilter_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IQueroFilter_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IQueroFilter_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IQueroFilter_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IQueroFilter_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IQueroFilter_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IQueroFilter_INTERFACE_DEFINED__ */



#ifndef __QUEROLib_LIBRARY_DEFINED__
#define __QUEROLib_LIBRARY_DEFINED__

/* library QUEROLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_QUEROLib;

EXTERN_C const CLSID CLSID_QueroBand;

#ifdef __cplusplus

class DECLSPEC_UUID("A411D7F4-8D11-43EF-BDE4-AA921666388A")
QueroBand;
#endif

EXTERN_C const CLSID CLSID_QueroFilter;

#ifdef __cplusplus

class DECLSPEC_UUID("65AD9A7A-9E52-43D2-AA3D-02FBC9E535B8")
QueroFilter;
#endif
#endif /* __QUEROLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


