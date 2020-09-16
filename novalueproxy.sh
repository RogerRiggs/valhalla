#!/bin/bash -x
# Compile WITHOUT value classes for lambda proxies plus debugging info
mkdir -p x
#DUMP=-J-Djdk.internal.lambda.dumpProxyClasses=x 
 
javac -J-DDEBUG=201 -J-Djdk.internal.lambda.inlineProxy=false $DUMP JavadocExamples.java

