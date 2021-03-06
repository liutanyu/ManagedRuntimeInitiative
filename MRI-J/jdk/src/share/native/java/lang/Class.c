/*
 * Copyright 1994-2004 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Sun designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Sun in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 */
// This file is a derivative work resulting from (and including) modifications
// made by Azul Systems, Inc.  The date of such changes is 2010.
// Copyright 2010 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, Inc., 1600 Plymouth Street, Mountain View,
// CA 94043 USA, or visit www.azulsystems.com if you need additional information
// or have any questions.

/*-
 *      Implementation of class Class
 *
 *      former threadruntime.c, Sun Sep 22 12:09:39 1991
 */

#include <string.h>
#include <stdlib.h>

#include "jni.h"
#include "jni_util.h"
#include "jvm.h"
#include "java_lang_Class.h"

/* defined in libverify.so/verify.dll (src file common/check_format.c) */
extern jboolean VerifyClassname(char *utf_name, jboolean arrayAllowed);
extern jboolean VerifyFixClassname(char *utf_name);

#define OBJ "Ljava/lang/Object;"
#define CLS "Ljava/lang/Class;"
#define CPL "Lsun/reflect/ConstantPool;"
#define STR "Ljava/lang/String;"
#define JCL "Ljava/lang/ClassLoader;"
#define FLD "Ljava/lang/reflect/Field;"
#define MHD "Ljava/lang/reflect/Method;"
#define CTR "Ljava/lang/reflect/Constructor;"
#define PD  "Ljava/security/ProtectionDomain;"
#define BA  "[B"

static JNINativeMethod methods[] = {
    {"getName0",         "()" STR,          (void *)&JVM_GetClassName},
    {"getSuperclass",    "()" CLS,          NULL},
    {"getInterfaces0",    "()[" CLS,         (void *)&JVM_GetClassInterfaces},
    {"getClassLoader0",  "()" JCL,          (void *)&JVM_GetClassLoader},
    {"isInterface",      "()Z",             (void *)&JVM_IsInterface},
    {"getSigners0",       "()[" OBJ,         (void *)&JVM_GetClassSigners},
    {"setSigners0",       "([" OBJ ")V",     (void *)&JVM_SetClassSigners},
    {"isArray",          "()Z",             (void *)&JVM_IsArrayClass},
    {"isPrimitive",      "()Z",             (void *)&JVM_IsPrimitiveClass},
    {"getComponentType", "()" CLS,          (void *)&JVM_GetComponentType},
    {"getModifiers",     "()I",             (void *)&JVM_GetClassModifiers},
    {"getDeclaredFields0","(Z)[" FLD,       (void *)&JVM_GetClassDeclaredFields},
    {"getDeclaredMethods0","(Z)[" MHD,      (void *)&JVM_GetClassDeclaredMethods},
    {"getDeclaredConstructors0","(Z)[" CTR, (void *)&JVM_GetClassDeclaredConstructors},
    {"getProtectionDomain0", "()" PD,       (void *)&JVM_GetProtectionDomain},
    {"setProtectionDomain0", "(" PD ")V",   (void *)&JVM_SetProtectionDomain},
    {"getDeclaredClasses0",  "()[" CLS,      (void *)&JVM_GetDeclaredClasses},
    {"getDeclaringClass0",   "()" CLS,      (void *)&JVM_GetDeclaringClass},
    {"getGenericSignature0", "()" STR,       (void *)&JVM_GetClassSignature},
    {"getRawAnnotations0",      "()" BA,        (void *)&JVM_GetClassAnnotations},
    {"getConstantPool0",     "()" CPL,       (void *)&JVM_GetClassConstantPool},
    {"desiredAssertionStatus0","("CLS")Z",(void *)&JVM_DesiredAssertionStatus},
    {"getEnclosingMethod0", "()[" OBJ,      (void *)&JVM_GetEnclosingMethodInfo}
};

#undef OBJ
#undef CLS
#undef STR
#undef JCL
#undef FLD
#undef MHD
#undef CTR
#undef PD

JNIEXPORT void JNICALL
Java_java_lang_Class_registerNatives(JNIEnv *env, jclass cls)
{
    methods[1].fnPtr = (void *)(*env)->GetSuperclass;
    (*env)->RegisterNatives(env, cls, methods,
                            sizeof(methods)/sizeof(JNINativeMethod));
}

JNIEXPORT jclass JNICALL
Java_java_lang_Class_forName0(JNIEnv *env, jclass this, jstring classname,
                              jboolean initialize, jobject loader)
{
    char *clname;
    jclass cls = 0;
    char buf[128];
    int len;
    int unicode_len;

    if (classname == NULL) {
        JNU_ThrowNullPointerException(env, 0);
        return 0;
    }

    len = (*env)->GetStringUTFLength(env, classname);
    unicode_len = (*env)->GetStringLength(env, classname);
    if (len >= sizeof(buf)) {
        clname = malloc(len + 1);
        if (clname == NULL) {
            JNU_ThrowOutOfMemoryError(env, NULL);
            return NULL;
        }
    } else {
        clname = buf;
    }
    (*env)->GetStringUTFRegion(env, classname, 0, unicode_len, clname);

    if (VerifyFixClassname(clname) == JNI_TRUE) {
        /* slashes present in clname, use name b4 translation for exception */
        (*env)->GetStringUTFRegion(env, classname, 0, unicode_len, clname);
        JNU_ThrowClassNotFoundException(env, clname);
        goto done;
    }

    if (!VerifyClassname(clname, JNI_TRUE)) {  /* expects slashed name */
        JNU_ThrowClassNotFoundException(env, clname);
        goto done;
    }

    cls = JVM_FindClassFromClassLoader(env, clname, initialize,
                                       loader, JNI_FALSE);

 done:
    if (clname != buf) {
        free(clname);
    }
    return cls;
}

JNIEXPORT jboolean JNICALL
Java_java_lang_Class_isInstance(JNIEnv *env, jobject cls, jobject obj)
{
    if (obj == NULL) {
        return JNI_FALSE;
    }
    return (*env)->IsInstanceOf(env, obj, (jclass)cls);
}

JNIEXPORT jboolean JNICALL
Java_java_lang_Class_isAssignableFrom(JNIEnv *env, jobject cls, jobject cls2)
{
    if (cls2 == NULL) {
        JNU_ThrowNullPointerException(env, 0);
        return JNI_FALSE;
    }
    return (*env)->IsAssignableFrom(env, cls2, cls);
}

JNIEXPORT jclass JNICALL
Java_java_lang_Class_getPrimitiveClass(JNIEnv *env,
                                       jclass cls,
                                       jstring name)
{
    const char *utfName;
    jclass result;

    if (name == NULL) {
        JNU_ThrowNullPointerException(env, 0);
        return NULL;
    }

    utfName = (*env)->GetStringUTFChars(env, name, 0);
    if (utfName == 0)
        return NULL;

    result = JVM_FindPrimitiveClass(env, utfName);

    (*env)->ReleaseStringUTFChars(env, name, utfName);

    return result;
}
