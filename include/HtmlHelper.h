#pragma once

#include <pgmspace.h>

static const char TEXT_PLAIN[] PROGMEM = "text/plain";
static const char TEXT_HTML[] PROGMEM = "text/html";
static const char TEXT_CSS[] PROGMEM = "text/css";
static const char TEXT_JS[] PROGMEM = "text/javascript";
static const char TEXT_JSON[] PROGMEM = "application/json";

static const char HTML_TAG_END[] PROGMEM = ">\n";
static const char HTML_PAGE_START[] PROGMEM = "<!DOCTYPE html>\n"
  "<html>\n"
  "<head>\n"
  "<title>";
static const char HTML_PAGE_CONT[] PROGMEM = "</title>\n"
  "<meta charset=\"utf-8\">\n"
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n";
static const char HTML_STYLE_START[] PROGMEM = "<style>\n";
static const char HTML_STYLE_END[] PROGMEM = "</style>\n";
static const char HTML_SCRIPT_START[] PROGMEM = "<script>\n";
static const char HTML_SCRIPT_END[] PROGMEM = "</script>\n";
static const char HTML_BODY_START[] PROGMEM = "</head>\n"
  "<body";
static const char HTML_BODY[] PROGMEM = "</head>\n"
  "<body>\n";
static const char HTML_PAGE_END[] PROGMEM = "</body>\n"
  "</html>";

static const char JS_VALIDATE_INT[] PROGMEM = "function validateInt(field,minval,maxval){\n"
  "let val=parseInt(field.value);\n"
  "if(isNaN(val)||(val<minval))\n"
  "field.value=minval;\n"
  "else if(val>maxval)\n"
  "field.value=maxval;\n"
  "}\n";
static const char JS_VALIDATE_FLOAT[] PROGMEM = "function validateFloat(field,minval,maxval){\n"
  "let val=parseFloat(field.value);\n"
  "if(isNaN(val)||(val<minval))\n"
  "field.value=minval;\n"
  "else if(val>maxval)\n"
  "field.value=maxval;\n"
  "}\n";
