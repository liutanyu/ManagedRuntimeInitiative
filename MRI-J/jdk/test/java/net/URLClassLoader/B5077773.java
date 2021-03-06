/*
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

import java.io.*;
import java.net.*;

public class B5077773 {

    public static void main(String[] args) throws Exception {
        URLClassLoader loader =  new URLClassLoader (new URL[] {new URL("file:foo.jar")});
        /* This test will fail if the file below is removed from rt.jar */
        InputStream is = loader.getResourceAsStream ("javax/swing/text/rtf/charsets/mac.txt");
        if (is == null) {
            System.out.println ("could not find mac.txt");
            return;
        }
        int c=0;
        while ((is.read()) != -1) {
            c++;
        }
        if (c == 26) /* size of bad file */  {
            throw new RuntimeException ("Wrong mac.txt file was loaded");
        }
    }
}
