#!/bin/bash -x
# Compile WITH value classes for lambda proxies plus debugging info
mkdir -p x
#DUMP=-J-Djdk.internal.lambda.dumpProxyClasses=x 

# DEBUG = negative 201 generates inline proxy only for 201'th proxy
# The one with the problem
javac -J-DDEBUG=-201 -J-Djdk.internal.lambda.inlineProxy=false $DUMP JavadocExamples.java

