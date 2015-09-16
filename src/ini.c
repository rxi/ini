/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ini.h"

struct ini_t {
  char *data;
  char *end;
};


/* Case insensitive string compare */
static int strcmpci(const char *a, const char *b) {
  for (;;) {
    int d = tolower(*a) - tolower(*b);
    if (d != 0 || !*a) {
      return d;
    }
    a++, b++;
  }
}

/* Returns the next string in the split data */
static char* next(ini_t *ini, char *p) {
  p += strlen(p);
  while (p < ini->end && *p == '\0') {
    p++;
  }
  return p;
}

/* Splits data in place into strings containing section-headers, keys and
 * values using one or more '\0' as a delimiter */
static void split_data(ini_t *ini) {
  char *q, *p = ini->data;

  while (p < ini->end) {
    switch (*p) {
      case '\r':
      case '\n':
      case '\t':
      case ' ':
        *p = '\0';
        /* Fall through */

      case '\0':
        p++;
        break;

      case '[':
        p += strcspn(p, "]");
        *p = '\0';
        break;

      case '=':
        do {
          *p++ = '\0';
        } while (*p == ' ' || *p == '\t');
        p += strcspn(p, "\n");
        goto trim_back;

      case ';':
        while (*p && *p != '\n') {
          *p++ = '\0';
        }
        break;

      default:
        p += strcspn(p, "=");
trim_back:
        q = p - 1;
        while (*q == ' ' || *q == '\t' || *q == '\r') {
          *q-- = '\0';
        }
        break;
    }
  }
}

/* Unescapes and unquotes all quoted strings in the split data */
static void unescape_quoted_strings(ini_t *ini) {
  char *p = ini->data;

  if (*p == '\0') {
    p = next(ini, p);
  }

  while (p < ini->end) {
    if (*p == '"') {
      /* Use `q` as write-head and `p` as read-head, `p` is always ahead of `q`
       * as escape sequences are always larger than their resultant data */
      char *q = p;
      p++;
      while (*q) {
        if (*p == '\\') {
          /* Handle escaped char */
          p++;
          switch (*p) {
            case 'r'  : *q = '\r';  break;
            case 'n'  : *q = '\n';  break;
            case 't'  : *q = '\t';  break;
            default   : *q = *p;    break;
          }

        } else if (*p == '"') {
          /* Handle end of string */
          *q = '\0';
          break;

        } else {
          /* Handle normal char */
          *q = *p;
        }
        q++, p++;
      }
      /* Fill gap between read-head and write-head's position with '\0' */
      p = next(ini, p);
      memset(q, '\0', p - q);
    } else {
      p = next(ini, p);
    }
  }
}


ini_t* ini_load(const char *filename) {
  ini_t *ini = NULL;
  FILE *fp = NULL;
  int n, sz;

  /* Init ini struct */
  ini = malloc(sizeof(*ini));
  if (!ini) {
    goto fail;
  }
  memset(ini, 0, sizeof(*ini));

  /* Open file */
  fp = fopen(filename, "rb");
  if (!fp) {
    goto fail;
  }

  /* Get file size */
  fseek(fp, 0, SEEK_END);
  sz = ftell(fp);
  rewind(fp);

  /* Load file content into memory, null terminate, init end var */
  ini->data = malloc(sz + 1);
  ini->data[sz] = '\0';
  ini->end = ini->data  + sz;
  n = fread(ini->data, sz, 1, fp);
  if (n !=  1) {
    goto fail;
  }

  /* Prepare data */
  split_data(ini);
  unescape_quoted_strings(ini);

  /* Clean up and return */
  fclose(fp);
  return ini;

fail:
  if (fp) fclose(fp);
  if (ini) ini_free(ini);
  return NULL;
}


void ini_free(ini_t *ini) {
  free(ini->data);
  free(ini);
}


const char* ini_get(ini_t *ini, const char *section, const char *key) {
  char *current_section = "";
  char *val;
  char *p = ini->data;

  if (*p == '\0') {
    p = next(ini, p);
  }

  while (p < ini->end) {
    if (*p == '[') {
      /* Handle section */
      current_section = p + 1;

    } else {
      /* Handle key */
      val = next(ini, p);
      if (!section || !strcmpci(section, current_section)) {
        if (!strcmpci(p, key)) {
          return val;
        }
      }
      p = val;
    }

    p = next(ini, p);
  }

  return NULL;
}


int ini_sget(
  ini_t *ini, const char *section, const char *value,
  const char *scanfmt, void *dst
) {
  const char *val = ini_get(ini, section, value);
  if (!val) {
    return 0;
  }
  if (scanfmt) {
    sscanf(val, scanfmt, dst);
  } else {
    *((const char**) dst) = val;
  }
  return 1;
}
