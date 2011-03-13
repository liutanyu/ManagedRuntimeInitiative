/*
 * Copyright 1998-1999 Sun Microsystems, Inc.  All Rights Reserved.
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

/*
 */
import java.rmi.*;
import java.rmi.server.*;

public class EchoImpl
    extends UnicastRemoteObject
    implements Echo
{
    private static final byte[] pattern = { (byte) 'A' };

    public EchoImpl(String protocol) throws RemoteException {
        super(0,
              new MultiSocketFactory.ClientFactory(protocol, pattern),
              new MultiSocketFactory.ServerFactory(protocol, pattern));
    }

    public byte[] echoNot(byte[] data) {
        byte[] result = new byte[data.length];
        for (int i = 0; i < data.length; i++)
            result[i] = (byte) ~data[i];
        return result;
    }

    public static void main(String[] args) {
        /*
         * The following line is required with the JDK 1.2 VM so that the
         * VM can exit gracefully when this test completes.  Otherwise, the
         * conservative garbage collector will find a handle to the server
         * object on the native stack and not clear the weak reference to
         * it in the RMI runtime's object table.
         */
        Object dummy = new Object();

        TestLibrary.suggestSecurityManager("java.rmi.RMISecurityManager");

        try {
            String protocol = "";
            if (args.length >= 1)
                protocol = args[0];

            System.out.println("EchoServer: creating remote object");
            EchoImpl impl = new EchoImpl(protocol);
            System.out.println("EchoServer: binding in registry");
            Naming.rebind("//:" + TestLibrary.REGISTRY_PORT +
                          "/EchoServer", impl);
            System.out.println("EchoServer ready.");
        } catch (Exception e) {
            System.err.println("EXCEPTION OCCURRED:");
            e.printStackTrace();
        }
    }
}