/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_CPPUHELPER_SOURCE_SERVICEMANAGER_HXX
#define INCLUDED_CPPUHELPER_SOURCE_SERVICEMANAGER_HXX

#include <sal/config.h>

#include <cassert>
#include <map>
#include <memory>
#include <vector>

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/container/XContentEnumerationAccess.hpp>
#include <com/sun/star/container/XSet.hpp>
#include <com/sun/star/lang/XEventListener.hpp>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/lang/XMultiComponentFactory.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XSingleServiceFactory.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/uno/Reference.hxx>
#include <cppuhelper/basemutex.hxx>
#include <cppuhelper/compbase.hxx>
#include <osl/mutex.hxx>
#include <registry/registry.hxx>
#include <rtl/ustring.hxx>
#include <cppuhelper/weak.hxx>

namespace com { namespace sun { namespace star { namespace lang {
    class XSingleComponentFactory;
} } } }
namespace cppu { struct ContextEntry_Init; }

namespace cppuhelper {

extern "C" {

typedef css::uno::XInterface * SAL_CALL ImplementationConstructorFn(
    css::uno::XComponentContext *, css::uno::Sequence<css::uno::Any> const &);

}

typedef std::function<css::uno::XInterface * (css::uno::XComponentContext *, css::uno::Sequence<css::uno::Any> const&)> WrapperConstructorFn;

typedef cppu::WeakComponentImplHelper<
    css::lang::XServiceInfo, css::lang::XMultiServiceFactory,
    css::lang::XMultiComponentFactory, css::container::XSet,
    css::container::XContentEnumerationAccess, css::beans::XPropertySet,
    css::beans::XPropertySetInfo, css::lang::XEventListener,
    css::lang::XInitialization>
ServiceManagerBase;

class ServiceManager:
    private cppu::BaseMutex, public ServiceManagerBase
{
public:
    struct Data {
        Data() = default;
        Data(const Data&) = delete;
        const Data& operator=(const Data&) = delete;

        struct ImplementationInfo {
            ImplementationInfo(
                rtl::OUString const & theName, rtl::OUString const & theLoader,
                rtl::OUString const & theUri,
                rtl::OUString const & theEnvironment,
                rtl::OUString const & theConstructor,
                rtl::OUString const & thePrefix,
                css::uno::Reference< css::uno::XComponentContext > const &
                    theAlienContext,
                rtl::OUString const & theRdbFile):
                name(theName), loader(theLoader), uri(theUri),
                environment(theEnvironment), constructor(theConstructor),
                prefix(thePrefix), alienContext(theAlienContext),
                rdbFile(theRdbFile)
            {}

            explicit ImplementationInfo(rtl::OUString const & theName):
                name(theName) {}

            ImplementationInfo(const ImplementationInfo&) = delete;
            const ImplementationInfo& operator=(const ImplementationInfo&) = delete;

            rtl::OUString const name;
            rtl::OUString const loader;
            rtl::OUString const uri;
            rtl::OUString const environment;
            rtl::OUString const constructor;
            rtl::OUString const prefix;
            css::uno::Reference< css::uno::XComponentContext > const
                alienContext;
            rtl::OUString const rdbFile;
            std::vector< rtl::OUString > services;
            std::vector< rtl::OUString > singletons;
        };

        struct Implementation {
            Implementation(
                rtl::OUString const & name, rtl::OUString const & loader,
                rtl::OUString const & uri, rtl::OUString const & environment,
                rtl::OUString const & constructorName,
                rtl::OUString const & prefix,
                css::uno::Reference< css::uno::XComponentContext > const &
                    alienContext,
                rtl::OUString const & rdbFile):
                info(
                    new ImplementationInfo(
                        name, loader, uri, environment, constructorName, prefix,
                        alienContext, rdbFile)),
                constructor(nullptr), status(STATUS_NEW), dispose(true)
            {}

            Implementation(
                rtl::OUString const & name,
                css::uno::Reference< css::lang::XSingleComponentFactory >
                    const & theFactory1,
                css::uno::Reference< css::lang::XSingleServiceFactory > const &
                    theFactory2,
                css::uno::Reference< css::lang::XComponent > const &
                    theComponent):
                info(new ImplementationInfo(name)), constructor(nullptr),
                factory1(theFactory1), factory2(theFactory2),
                component(theComponent), status(STATUS_LOADED), dispose(true)
            { assert(theFactory1.is() || theFactory2.is()); }

            Implementation(const Implementation&) = delete;
            const Implementation& operator=(const Implementation&) = delete;

            css::uno::Reference<css::uno::XInterface> createInstance(
                css::uno::Reference<css::uno::XComponentContext> const &
                    context,
                bool singletonRequest);

            css::uno::Reference<css::uno::XInterface>
            createInstanceWithArguments(
                css::uno::Reference<css::uno::XComponentContext> const &
                    context,
                bool singletonRequest,
                css::uno::Sequence<css::uno::Any> const & arguments);

            enum Status { STATUS_NEW, STATUS_WRAPPER, STATUS_LOADED };

            std::shared_ptr< ImplementationInfo > info;
            WrapperConstructorFn constructor;
            css::uno::Reference< css::lang::XSingleComponentFactory > factory1;
            css::uno::Reference< css::lang::XSingleServiceFactory > factory2;
            css::uno::Reference< css::lang::XComponent > component;
            Status status;

            osl::Mutex mutex;
            css::uno::Reference< css::lang::XComponent > disposeSingleton;
            bool dispose;

        private:
            void updateDisposeSingleton(
                bool singletonRequest,
                css::uno::Reference<css::uno::XInterface> const & instance);
        };

        typedef std::map< rtl::OUString, std::shared_ptr< Implementation > >
            NamedImplementations;

        typedef
            std::map<
                css::uno::Reference< css::lang::XServiceInfo >,
                std::shared_ptr< Implementation > >
            DynamicImplementations;

        typedef
            std::map<
                rtl::OUString,
                std::vector< std::shared_ptr< Implementation > > >
            ImplementationMap;

        NamedImplementations namedImplementations;
        DynamicImplementations dynamicImplementations;
        ImplementationMap services;
        ImplementationMap singletons;
    };

    ServiceManager(): ServiceManagerBase(m_aMutex) {}

    ServiceManager(const ServiceManager&) = delete;
    const ServiceManager& operator=(const ServiceManager&) = delete;

    using ServiceManagerBase::acquire;
    using ServiceManagerBase::release;

    void init(rtl::OUString const & rdbUris);

    void setContext(
        css::uno::Reference< css::uno::XComponentContext > const & context)
    {
        assert(context.is());
        assert(!context_.is());
        context_ = context;
    }

    void addSingletonContextEntries(
        std::vector< cppu::ContextEntry_Init > * entries);

    css::uno::Reference< css::uno::XComponentContext > const & getContext()
        const
    {
        assert(context_.is());
        return context_;
    }

    void loadImplementation(
        css::uno::Reference< css::uno::XComponentContext > const & context,
        std::shared_ptr< Data::Implementation > const & implementation);

private:
    virtual ~ServiceManager() override;

    virtual void SAL_CALL disposing() override;

    virtual rtl::OUString SAL_CALL getImplementationName() override;

    virtual sal_Bool SAL_CALL supportsService(rtl::OUString const & ServiceName) override;

    virtual css::uno::Sequence< rtl::OUString > SAL_CALL
    getSupportedServiceNames() override;

    virtual css::uno::Reference< css::uno::XInterface > SAL_CALL createInstance(
        rtl::OUString const & aServiceSpecifier) override;

    virtual css::uno::Reference< css::uno::XInterface > SAL_CALL
    createInstanceWithArguments(
        rtl::OUString const & ServiceSpecifier,
        css::uno::Sequence< css::uno::Any > const & Arguments) override;

    virtual css::uno::Sequence< rtl::OUString > SAL_CALL
    getAvailableServiceNames() override;

    virtual css::uno::Reference< css::uno::XInterface > SAL_CALL
    createInstanceWithContext(
        rtl::OUString const & aServiceSpecifier,
        css::uno::Reference< css::uno::XComponentContext > const & Context) override;

    virtual css::uno::Reference< css::uno::XInterface > SAL_CALL
    createInstanceWithArgumentsAndContext(
        rtl::OUString const & ServiceSpecifier,
        css::uno::Sequence< css::uno::Any > const & Arguments,
        css::uno::Reference< css::uno::XComponentContext > const & Context) override;

    virtual css::uno::Type SAL_CALL getElementType() override;

    virtual sal_Bool SAL_CALL hasElements() override;

    virtual css::uno::Reference< css::container::XEnumeration > SAL_CALL
    createEnumeration() override;

    virtual sal_Bool SAL_CALL has(css::uno::Any const & aElement) override;

    virtual void SAL_CALL insert(css::uno::Any const & aElement) override;

    virtual void SAL_CALL remove(css::uno::Any const & aElement) override;

    virtual css::uno::Reference< css::container::XEnumeration > SAL_CALL
    createContentEnumeration(rtl::OUString const & aServiceName) override;

    virtual css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL
    getPropertySetInfo() override;

    virtual void SAL_CALL setPropertyValue(
        rtl::OUString const & aPropertyName, css::uno::Any const & aValue) override;

    virtual css::uno::Any SAL_CALL getPropertyValue(
        rtl::OUString const & PropertyName) override;

    virtual void SAL_CALL addPropertyChangeListener(
        rtl::OUString const & aPropertyName,
        css::uno::Reference< css::beans::XPropertyChangeListener > const &
            xListener) override;

    virtual void SAL_CALL removePropertyChangeListener(
        rtl::OUString const & aPropertyName,
        css::uno::Reference< css::beans::XPropertyChangeListener > const &
            aListener) override;

    virtual void SAL_CALL addVetoableChangeListener(
        rtl::OUString const & PropertyName,
        css::uno::Reference< css::beans::XVetoableChangeListener > const &
            aListener) override;

    virtual void SAL_CALL removeVetoableChangeListener(
        rtl::OUString const & PropertyName,
        css::uno::Reference< css::beans::XVetoableChangeListener > const &
            aListener) override;

    virtual css::uno::Sequence< css::beans::Property > SAL_CALL getProperties() override;

    virtual css::beans::Property SAL_CALL getPropertyByName(
        rtl::OUString const & aName) override;

    virtual sal_Bool SAL_CALL hasPropertyByName(rtl::OUString const & Name) override;

    virtual void SAL_CALL disposing(css::lang::EventObject const & Source) override;

    virtual void SAL_CALL initialize(
        css::uno::Sequence<css::uno::Any> const & aArguments)
        override;

    // needs to be called with rBHelper.rMutex locked:
    bool isDisposed() { return rBHelper.bDisposed || rBHelper.bInDispose; }

    void removeEventListenerFromComponent(
        css::uno::Reference< css::lang::XComponent > const & component);

    void readRdbDirectory(rtl::OUString const & uri, bool optional);

    void readRdbFile(rtl::OUString const & uri, bool optional);

    bool readLegacyRdbFile(rtl::OUString const & uri);

    rtl::OUString readLegacyRdbString(
        rtl::OUString const & uri, RegistryKey & key,
        rtl::OUString const & path);

    void readLegacyRdbStrings(
        rtl::OUString const & uri, RegistryKey & key,
        rtl::OUString const & path, std::vector< rtl::OUString > * strings);

    void insertRdbFiles(
        std::vector< rtl::OUString > const & uris,
        css::uno::Reference< css::uno::XComponentContext > const &
            alientContext);

    void insertLegacyFactory(
        css::uno::Reference< css::lang::XServiceInfo > const & factoryInfo);

    bool insertExtraData(Data const & extra);

    void removeRdbFiles(std::vector< rtl::OUString > const & uris);

    bool removeLegacyFactory(
        css::uno::Reference< css::lang::XServiceInfo > const & factoryInfo,
        bool removeListener);

    void removeImplementation(const rtl::OUString & name);

    std::shared_ptr< Data::Implementation > findServiceImplementation(
        css::uno::Reference< css::uno::XComponentContext > const & context,
        rtl::OUString const & specifier);

    void preloadImplementations();

    css::uno::Reference< css::uno::XComponentContext > context_;
    Data data_;
};

}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
