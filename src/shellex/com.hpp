/**
 * @file shellex/com.hpp
 *
 * @copyright 2018-2020 Bill Zissimopoulos
 */
/*
 * This file is part of WinSpd.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef WINSPD_SHELLEX_COM_HPP_INCLUDED
#define WINSPD_SHELLEX_COM_HPP_INCLUDED

#include <objbase.h>
#include <new>
#include <type_traits>

inline ULONG GlobalRefCnt(ULONG Value)
{
    static LONG RefCnt;
    if (0 == Value)
        return RefCnt;
    else
        return InterlockedAdd(&RefCnt, Value);
}

template <typename... Interfaces>
class CoObject : public Interfaces...
{
public:
    /* IUnknown */
    STDMETHODIMP QueryInterface(REFIID Iid, void **PObject)
    {
        return QueryInterfaces<Interfaces...>(Iid, PObject);
    }
    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_RefCnt);
    }
    STDMETHODIMP_(ULONG) Release()
    {
        ULONG RefCnt = InterlockedDecrement(&_RefCnt);
        if (0 == RefCnt)
            delete this;
        return RefCnt;
    }

    /* internal interface */
    static STDMETHODIMP Register(BOOL Flag)
    {
        return S_OK;
    }
    CoObject() : _RefCnt(1)
    {
        GlobalRefCnt(+1);
    }
    virtual ~CoObject()
    {
        GlobalRefCnt(-1);
    }

private:
    template <typename Interface, typename... Interfaces>
    STDMETHODIMP QueryInterfaces(REFIID Iid, void **PObject)
    {
        if (__uuidof(Interface) == Iid || __uuidof(IUnknown) == Iid)
        {
            AddRef();
            *PObject = static_cast<Interface *>(this);
            return S_OK;
        }
        else
            return QueryInterfaces<Interfaces...>(Iid, PObject);
    }
    template <typename... Interfaces>
    typename std::enable_if<0 == sizeof...(Interfaces), HRESULT>::type STDMETHODCALLTYPE
        QueryInterfaces(REFIID Iid, void **PObject)
    {
        *PObject = 0;
        return E_NOINTERFACE;
    }

private:
    ULONG _RefCnt;
};

class CoClass : public IClassFactory
{
public:
    /* IUnknown */
    STDMETHODIMP QueryInterface(REFIID Iid, void **PObject)
    {
        if (__uuidof(IClassFactory) == Iid || __uuidof(IUnknown) == Iid)
        {
            AddRef();
            *PObject = static_cast<IClassFactory *>(this);
            return S_OK;
        }
        else
        {
            *PObject = 0;
            return E_NOINTERFACE;
        }
    }
    STDMETHODIMP_(ULONG) AddRef()
    {
        return 2;
    }
    STDMETHODIMP_(ULONG) Release()
    {
        return 1;
    }

    /* IClassFactory */
    STDMETHODIMP CreateInstance(IUnknown *Outer, REFIID Iid, void **PObject)
    {
        if (0 != Outer)
        {
            *PObject = 0;
            return CLASS_E_NOAGGREGATION;
        }
        return _CreateInstance(Iid, PObject);
    }
    STDMETHODIMP LockServer(BOOL Lock)
    {
        GlobalRefCnt(Lock ? +1 : -1);
        return S_OK;
    }

    /* internal interface */
    CoClass(REFCLSID Clsid, PWSTR ThreadingModel,
        HRESULT (STDMETHODCALLTYPE *Register)(BOOL Flag),
        HRESULT (STDMETHODCALLTYPE *CreateInstance)(REFIID Iid, LPVOID *PObject)) :
        _Next(0), _Clsid(Clsid), _ThreadingModel(ThreadingModel),
        _Register(Register),
        _CreateInstance(CreateInstance)
    {
        _Next = ClassList();
        ClassList() = this;
    }
    static STDMETHODIMP GetClassObject(REFCLSID Clsid, REFIID Iid, LPVOID *PObject)
    {
        for (CoClass *Class = ClassList(); 0 != Class; Class = Class->_Next)
            if (Clsid == Class->_Clsid)
                return Class->QueryInterface(Iid, PObject);
        *PObject = 0;
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    static STDMETHODIMP CanUnloadNow()
    {
        return 0 == GlobalRefCnt(0) ? S_OK : S_FALSE;
    }
    static STDMETHODIMP RegisterServer()
    {
        HMODULE Module;
        WCHAR ModuleFileName[MAX_PATH], GuidStr[40];
        HKEY ClassesKey, GuidKey, InProcKey;
        HRESULT Result;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (PWSTR)RegisterServer, &Module))
            return HRESULT_FROM_WIN32(GetLastError());
        if (0 == GetModuleFileNameW(Module, ModuleFileName, MAX_PATH))
            return HRESULT_FROM_WIN32(GetLastError());
        Result = HRESULT_FROM_WIN32(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\CLSID",
            0, KEY_ALL_ACCESS, &ClassesKey));
        if (S_OK != Result)
            return Result;
        for (CoClass *Class = ClassList(); 0 != Class; Class = Class->_Next)
        {
            StringFromGUID2(Class->_Clsid, GuidStr, sizeof GuidStr / sizeof GuidStr[0]);
            GuidKey = InProcKey = 0;
            Result = HRESULT_FROM_WIN32(RegCreateKeyExW(ClassesKey, GuidStr,
                0, 0, 0, KEY_ALL_ACCESS, 0, &GuidKey, 0));
            if (S_OK != Result)
                goto loop_exit;
            Result = HRESULT_FROM_WIN32(RegCreateKeyExW(GuidKey, L"InProcServer32",
                0, 0, 0, KEY_ALL_ACCESS, 0, &InProcKey, 0));
            if (S_OK != Result)
                goto loop_exit;
            Result = HRESULT_FROM_WIN32(RegSetValueExW(InProcKey,
                0, 0, REG_SZ,
                (BYTE *)ModuleFileName,
                (lstrlenW(ModuleFileName) + 1) * sizeof(WCHAR)));
            if (S_OK != Result)
                goto loop_exit;
            if (0 != Class->_ThreadingModel)
            {
                Result = HRESULT_FROM_WIN32(RegSetValueExW(InProcKey,
                    L"ThreadingModel", 0, REG_SZ,
                    (BYTE *)Class->_ThreadingModel,
                    (lstrlenW(Class->_ThreadingModel) + 1) * sizeof(WCHAR)));
                if (S_OK != Result)
                    goto loop_exit;
            }
            if (0 != Class->_Register)
                Result = Class->_Register(TRUE);
        loop_exit:
            if (0 != InProcKey)
                RegCloseKey(InProcKey);
            if (0 != GuidKey)
                RegCloseKey(GuidKey);
            if (S_OK != Result)
                break;
        }
        RegCloseKey(ClassesKey);
        return Result;
    }
    static STDMETHODIMP UnregisterServer()
    {
        WCHAR GuidStr[40];
        HKEY ClassesKey;
        HRESULT Result;
        Result = HRESULT_FROM_WIN32(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\CLSID",
            0, KEY_ALL_ACCESS, &ClassesKey));
        if (S_OK != Result)
            return Result;
        for (CoClass *Class = ClassList(); 0 != Class; Class = Class->_Next)
        {
            if (0 != Class->_Register)
                Class->_Register(FALSE);
            StringFromGUID2(Class->_Clsid, GuidStr, sizeof GuidStr / sizeof GuidStr[0]);
            RegDeleteTreeW(ClassesKey, GuidStr);
        }
        RegCloseKey(ClassesKey);
        return Result;
    }

private:
    static CoClass *&ClassList()
    {
        static CoClass *ClassList;
        return ClassList;
    }

private:
    CoClass *_Next;
    CLSID _Clsid;
    PWSTR _ThreadingModel;
    HRESULT (STDMETHODCALLTYPE *_Register)(BOOL Flag);
    HRESULT (STDMETHODCALLTYPE *_CreateInstance)(REFIID Iid, LPVOID *PObject);
};

template <typename T>
class CoClassOf : public CoClass
{
public:
    CoClassOf() : CoClass(T::Clsid, T::ThreadingModel, T::Register, CreateInstance)
    {
    }

private:
    static STDMETHODIMP CreateInstance(REFIID Iid, void **PObject)
    {
        T *Object = new (std::nothrow) T;
        if (0 == Object)
        {
            *PObject = 0;
            return E_OUTOFMEMORY;
        }
        HRESULT Result = Object->QueryInterface(Iid, PObject);
        Object->Release();
        return Result;
    }
};

#endif
