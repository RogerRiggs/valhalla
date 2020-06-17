/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
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
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

import java.io.IOException;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.net.http.HttpResponse.BodyHandlers;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Flow;
import java.util.regex.Pattern;

public class JavadocExamples {
    HttpRequest request = null;
    HttpClient client = null;

    void fromLineSubscriber2() throws IOException, InterruptedException {
        // A LineParserSubscriber that implements Flow.Subscriber<String>
        // and accumulates lines that match a particular pattern
        LineParserSubscriber subscriber = new LineParserSubscriber();
        HttpResponse<List<String>> response = client.send(request,
                BodyHandlers.fromLineSubscriber(subscriber, s -> s.getMatchingLines(), "\n"));
    }

    static final class LineParserSubscriber implements Flow.Subscriber<String> {
        LineParserSubscriber() {}
        @Override
        public void onSubscribe(Flow.Subscription subscription) {}
        @Override
        public void onNext(String item) {}
        @Override
        public void onError(Throwable throwable) {}
        @Override
        public void onComplete() {}
        public List<String> getMatchingLines() {
            return null;
        }
    }

}
