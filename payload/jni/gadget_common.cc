#include <new>

#include "gadget.h"
#include "jni_except-inl.h"


// The cached symbols delivered from injector.
char* g_module_path;
char* g_lib_path;
char* g_class_name;

// The cached Java VM handle.
JavaVM* g_jvm;

// The original entry to IndirectReferenceTable::Add().
void* g_indirect_reference_table_add;

// The original entry to IndirectReferneceTable::Remove().
void* g_indirect_reference_table_remove;

// The original entry to Thread::DecodeJObject().
void* g_thread_decode_jobject;

// The original entry to the loadClass() quick compiled code.
void* g_load_class_quick_compiled;

// The cached class and object instance of analysis module.
jclass g_class_analysis_main;
jobject g_obj_analysis_main;

// The global map to maintain the information about all the instrumented methods
// of the target app.
PtrBundleMap g_map_method_bundle(nullptr);

// The global map to cache the access information about all the wrappers of
// primitive Java types.
PtrPrimitiveMap g_map_primitive_wrapper(nullptr);

// The global map to cache the frequently used method ids.
PtrClassMap g_map_class_cache(nullptr);


void InstrumentGadgetComposer::Compose()
{
    if (LinkWithAnalysisAPK() != PROC_SUCC)
        return;

    // Initialize the global map for method information maintenance.
    typedef std::unordered_map<jmethodID, std::unique_ptr<MethodBundleNative>>
            BundleMap;
    BundleMap* bundle_map = new(std::nothrow)BundleMap();
    if (!bundle_map)
        return;
    g_map_method_bundle.reset(bundle_map);

    // Invoke the overrode "void Instrument.onApplicationStart()" to let the
    // analysis APK register the methods which it intends to instrument.
    if (RegisterInstrumentGadget() != PROC_SUCC)
        return;

    return;
}

bool InstrumentGadgetComposer::LinkWithAnalysisAPK()
{
    // Resolve "void Instrument.prepareNativeBridge(String)".
    char sig[kBlahSizeMid];
    snprintf(sig, kBlahSizeMid, "(%s)%c", kSigString, kSigVoid);
    jmethodID meth = env_->GetMethodID(g_class_analysis_main,
                                       kFuncPrepareNativeBridge, sig);
    CHK_EXCP(env_);

    // Convert the library pathname to UTF format.
    jstring path_module = env_->NewStringUTF(g_lib_path);
    CHK_EXCP(env_);

    // Invoke it to let the analysis APK link with this native library.
    env_->CallVoidMethod(g_obj_analysis_main, meth, path_module);
    CHK_EXCP(env_);

    env_->DeleteLocalRef(path_module);
    return PROC_SUCC;
}

bool InstrumentGadgetComposer::RegisterInstrumentGadget()
{
    // Resolve the overrode "void Instrument.onApplicationStart()".
    char sig[kBlahSizeMid];
    snprintf(sig, kBlahSizeMid, "()%c", kSigVoid);
    jmethodID meth = env_->GetMethodID(g_class_analysis_main,
                                       kFuncOnApplicationStart, sig);
    CHK_EXCP(env_);

    // Invoke it to let the analysis APK deploy the instrument gadget.
    env_->CallVoidMethod(g_obj_analysis_main, meth);
    CHK_EXCP(env_);

    return PROC_SUCC;
}

MethodBundleNative::~MethodBundleNative()
{
    JNIEnv* env;
    g_jvm->AttachCurrentThread(&env, nullptr);
    env->DeleteGlobalRef(bundle_);
}

PrimitiveTypeWrapper::~PrimitiveTypeWrapper()
{
    JNIEnv* env;
    g_jvm->AttachCurrentThread(&env, nullptr);
    env->DeleteGlobalRef(clazz_);
}

bool PrimitiveTypeWrapper::LoadWrappers(JNIEnv* env)
{
    // Load Boolean wrapper.
    char sig[kBlahSizeMid];
    jclass clazz = env->FindClass(kNormBooleanObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigBoolean, kSigVoid);
    jmethodID meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigBoolean);
    jmethodID meth_access = env->GetMethodID(clazz, kFuncBooleanValue, sig);

    jobject g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    jclass g_clazz = reinterpret_cast<jclass>(g_ref);
    PrimitiveTypeWrapper* wrapper = new(std::nothrow) PrimitiveTypeWrapper(
                                        g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeBoolean,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    // Load Byte Wrapper.
    clazz = env->FindClass(kNormByteObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigByte, kSigVoid);
    meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigByte);
    meth_access = env->GetMethodID(clazz, kFuncByteValue, sig);

    g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    g_clazz = reinterpret_cast<jclass>(g_ref);
    wrapper = new(std::nothrow) PrimitiveTypeWrapper(g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeByte,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    // Load Character Wrapper.
    clazz = env->FindClass(kNormCharObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigChar, kSigVoid);
    meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigChar);
    meth_access = env->GetMethodID(clazz, kFuncCharValue, sig);

    g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    g_clazz = reinterpret_cast<jclass>(g_ref);
    wrapper = new(std::nothrow) PrimitiveTypeWrapper(g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeChar,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    // Load Short Wrapper.
    clazz = env->FindClass(kNormShortObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigShort, kSigVoid);
    meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigShort);
    meth_access = env->GetMethodID(clazz, kFuncShortValue, sig);

    g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    g_clazz = reinterpret_cast<jclass>(g_ref);
    wrapper = new(std::nothrow) PrimitiveTypeWrapper(g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeShort,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    // Load Integer Wrapper.
    clazz = env->FindClass(kNormIntObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigInt, kSigVoid);
    meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigInt);
    meth_access = env->GetMethodID(clazz, kFuncIntValue, sig);

    g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    g_clazz = reinterpret_cast<jclass>(g_ref);
    wrapper = new(std::nothrow) PrimitiveTypeWrapper(g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeInt,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    // Load Float Wrapper.
    clazz = env->FindClass(kNormFloatObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigFloat, kSigVoid);
    meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigFloat);
    meth_access = env->GetMethodID(clazz, kFuncFloatValue, sig);

    g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    g_clazz = reinterpret_cast<jclass>(g_ref);
    wrapper = new(std::nothrow) PrimitiveTypeWrapper(g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeFloat,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    // Load Long Wrapper.
    clazz = env->FindClass(kNormLongObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigLong, kSigVoid);
    meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigLong);
    meth_access = env->GetMethodID(clazz, kFuncLongValue, sig);

    g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    g_clazz = reinterpret_cast<jclass>(g_ref);
    wrapper = new(std::nothrow) PrimitiveTypeWrapper(g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeLong,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    // Load Double Wrapper.
    clazz = env->FindClass(kNormDoubleObject);
    snprintf(sig, kBlahSizeMid, "(%c)%c", kSigDouble, kSigVoid);
    meth_ctor = env->GetMethodID(clazz, kFuncConstructor, sig);
    snprintf(sig, kBlahSizeMid, "()%c", kSigDouble);
    meth_access = env->GetMethodID(clazz, kFuncDoubleValue, sig);

    g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
    g_clazz = reinterpret_cast<jclass>(g_ref);
    wrapper = new(std::nothrow) PrimitiveTypeWrapper(g_clazz, meth_ctor, meth_access);
    // TODO: Out of memory check and message logging.
    if (!wrapper)
        return PROC_FAIL;
    g_map_primitive_wrapper->insert(std::make_pair(kTypeDouble,
                               std::unique_ptr<PrimitiveTypeWrapper>(wrapper)));

    return PROC_SUCC;
}

ClassCache::~ClassCache()
{
    JNIEnv* env;
    g_jvm->AttachCurrentThread(&env, nullptr);
    env->DeleteGlobalRef(clazz_);
}

bool ClassCache::LoadClasses(JNIEnv* env)
{
    char sig[kBlahSizeMid];
    // Load "java.lang.Object".
    {
        jclass clazz = env->FindClass(kNormObject);
        jobject g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
        jclass g_clazz = reinterpret_cast<jclass>(g_ref);
        std::unique_ptr<ClassCache> class_cache(new(std::nothrow)ClassCache(g_clazz));
        if (!class_cache.get())
            return PROC_FAIL;

        // Load "String Object.toString()".
        {
            snprintf(sig, kBlahSizeMid, "()%s", kSigString);
            jmethodID meth = env->GetMethodID(clazz, kFuncToString, sig);
            snprintf(sig, kBlahSizeMid, "%s()%s", kFuncToString, kSigString);
            std::string sig_method(sig);
            class_cache->CacheMethod(sig_method, meth);
        }
        std::string sig_class(kNormObject);
        g_map_class_cache->insert(std::make_pair(sig_class,
                            std::unique_ptr<ClassCache>(class_cache.release())));
    }

    // Load "java.lang.IllegalArgumentException".
    {
        jclass clazz = env->FindClass(kNormIllegalArgument);
        jobject g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
        jclass g_clazz = reinterpret_cast<jclass>(g_ref);
        std::unique_ptr<ClassCache> class_cache(new(std::nothrow)ClassCache(g_clazz));
        if (!class_cache.get())
            return PROC_FAIL;
        std::string sig_class(kNormIllegalArgument);
        g_map_class_cache->insert(std::make_pair(sig_class,
                            std::unique_ptr<ClassCache>(class_cache.release())));
    }

    // Load "java.lang.CLassNotFoundException".
    {
        jclass clazz = env->FindClass(kNormClassNotFound);
        jobject g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
        jclass g_clazz = reinterpret_cast<jclass>(g_ref);
        std::unique_ptr<ClassCache> class_cache(new(std::nothrow)ClassCache(g_clazz));
        if (!class_cache.get())
            return PROC_FAIL;
        std::string sig_class(kNormClassNotFound);
        g_map_class_cache->insert(std::make_pair(sig_class,
                            std::unique_ptr<ClassCache>(class_cache.release())));
    }

    // Load "java.lang.NoSuchMethodException".
    {
        jclass clazz = env->FindClass(kNormNoSuchMethod);
        jobject g_ref = env->NewGlobalRef(reinterpret_cast<jobject>(clazz));
        jclass g_clazz = reinterpret_cast<jclass>(g_ref);
        std::unique_ptr<ClassCache> class_cache(new(std::nothrow)ClassCache(g_clazz));
        if (!class_cache.get())
            return PROC_FAIL;
        std::string sig_class(kNormNoSuchMethod);
        g_map_class_cache->insert(std::make_pair(sig_class,
                            std::unique_ptr<ClassCache>(class_cache.release())));
    }

    return PROC_SUCC;
}