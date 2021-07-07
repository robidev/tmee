/* config file parser */
/* greg kennedy 2012 */

#include "cfg_parse.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>

/* Configuration list structures */
struct cfg_node
{
  char *key;
  char *value;

  struct cfg_node *next;
};

struct cfg_struct
{
  struct cfg_node *head;
};

/* Helper functions */
/*  A malloc() wrapper which handles null return values */
static void *cfg_malloc(unsigned int size)
{
  void *temp = malloc(size);
  if (temp == NULL)
  {
    fprintf(stderr,"CFG_PARSE ERROR: MALLOC(%u) returned NULL (errno=%d)\n",size,errno);
    exit(EXIT_FAILURE);
  }
  return temp;
}

/* Returns a duplicate of input str, without leading / trailing whitespace
    Input str *MUST* be null-terminated, or disaster will result */
static char *cfg_trim(const char *str)
{
  char *tstr = NULL;
  char *temp = (char *)str;

  int temp_len;

  /* advance start pointer to first non-whitespace char */
  while (*temp == ' ' || *temp == '\t' || *temp == '\n')
    temp ++;

  /* calculate length of output string, minus whitespace */
  temp_len = strlen(temp);
  while (temp_len > 0 && (temp[temp_len-1] == ' ' || temp[temp_len-1] == '\t' || temp[temp_len-1] == '\n'))
    temp_len --;

  /* copy portion of string to new string */
  tstr = (char *)malloc(temp_len + 1);
  tstr[temp_len] = '\0';
  memcpy(tstr,temp,temp_len);

  return tstr;
}

/* Load into cfg from a file.  Maximum line size is CFG_MAX_LINE-1 bytes... */
int cfg_load(struct cfg_struct *cfg, const char *filename)
{
  char buffer[CFG_MAX_LINE], *delim;
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) return -1;

  while (!feof(fp))
  {
    if (fgets(buffer,CFG_MAX_LINE,fp) != NULL)
    {
      /* locate first # sign and terminate string there (comment) */
      delim = strchr(buffer, '#');
      if (delim != NULL) *delim = '\0';

      /* locate first = sign and prepare to split */
      delim = strchr(buffer, '=');
      if (delim != NULL)
      {
        *delim = '\0';
        delim ++;

        cfg_set(cfg,buffer,delim);
      }
    }
  }

  fclose(fp);
  return 0;
}

/* Save complete cfg to file */
int cfg_save(struct cfg_struct *cfg, const char *filename)
{
  struct cfg_node *temp = cfg->head;

  FILE *fp = fopen(filename, "w");
  if (fp == NULL) return -1;

  while (temp != NULL)
  {
    if (fprintf(fp,"%s=%s\n",temp->key,temp->value) < 0) { 
      fclose(fp);
      return -2;
    }
    temp = temp->next;
  }
  fclose(fp);
  return 0;
}

/* Get option from cfg_struct */
const char * cfg_get(struct cfg_struct *cfg, const char *key)
{
  struct cfg_node *temp = cfg->head;

  char *tkey = cfg_trim(key);

  while (temp != NULL)
  {
    if (strcmp(tkey, temp->key) == 0)
    {
      free(tkey);
      return temp->value;
    }
    temp = temp->next;
  }

  free(tkey);
  return NULL;
}

/* Set option in cfg_struct */
void cfg_set(struct cfg_struct *cfg, const char *key, const char *value)
{
  char *tkey, *tvalue;

  struct cfg_node *temp = cfg->head;

  /* Trim key. */
  tkey = cfg_trim(key);

  /* Exclude empty key */
  if (strcmp(tkey,"") == 0) { free(tkey); return; }

  /* Trim value. */
  tvalue = cfg_trim(value);

  /* Depending on implementation, you may wish to treat blank value
     as a "delete" operation */
  /* if (strcmp(tvalue,"") == 0) { free(tvalue); free(tkey); cfg_delete(cfg,key); return; } */

  /* search list for existing key */
  while (temp != NULL)
  {
    if (strcmp(tkey, temp->key) == 0)
    {
      /* found a match: no longer need temp key */
      free(tkey);

      /* update value */
      free(temp->value);
      temp->value = tvalue;
      return;
    }
    temp = temp->next;
  }

  /* not found: create new element */
  temp = (struct cfg_node *)cfg_malloc(sizeof(struct cfg_node));

  /* assign key, value */
  temp->key = tkey;
  temp->value = tvalue;

  /* prepend */
  temp->next = cfg->head;
  cfg->head = temp;
}

/* Remove option in cfg_struct */
void cfg_delete(struct cfg_struct *cfg, const char *key)
{
  struct cfg_node *temp = cfg->head, *temp2 = NULL;

  char *tkey = cfg_trim(key);

  /* search list for existing key */
  while (temp != NULL)
  {
    if (strcmp(tkey, temp->key) == 0)
    {
      /* cleanup trimmed key */
      free(tkey);

      if (temp2 == NULL)
      {
        /* first element */
        cfg->head = temp->next;
      } else {
        /* splice out element */
        temp2->next = temp->next;
      }

      /* delete element */
      free(temp->value);
      free(temp->key);
      free(temp);

      return;
    }

    temp2 = temp;
    temp = temp->next;
  }

  /* not found */
  /* cleanup trimmed key */
  free(tkey);
}

/* Create a cfg_struct */
struct cfg_struct * cfg_init()
{
  struct cfg_struct *temp;
  temp = (struct cfg_struct *)cfg_malloc(sizeof(struct cfg_struct));
  temp->head = NULL;
  return temp;
}

/* Free a cfg_struct */
void cfg_free(struct cfg_struct *cfg)
{
  struct cfg_node *temp = NULL, *temp2;
  temp = cfg->head;
  while (temp != NULL)
  {
    temp2 = temp->next;
    free(temp->key);
    free(temp->value);
    free(temp);
    temp = temp2;
  }
  free (cfg);
}

int cfg_get_int(struct cfg_struct *config, const char * key, int *value)
{
  if(value == NULL)
  {
    printf("ERROR: value should be pointer\n");
    return -1;
  }
  const char *value_i = cfg_get(config, key);
  if(value_i == NULL)
  {
    printf("ERROR: could not find %s value in config file\n", key);
    return -1;
  }
  if(sscanf(value_i, "%d", value) != 1)
  {
    printf("ERROR: could not parse %s value: %s\n", key, cfg_get(config, key));
    return -1;
  }
  return 0;
}

int cfg_get_string(struct cfg_struct *config, const char * key, char **value)
{
  if(*value != NULL)
  {
    printf("ERROR: *value should be NULL\n");
    return -1;
  }
  const char *value_i = cfg_get(config, key);
  if(value_i == NULL)
  {
    printf("ERROR: could not find %s value in config file\n", key);
    return -1;
  }
  if(sscanf(value_i, "\"%m[^\"]\"", value) != 1)
  {
    printf("ERROR: could not parse %s value: %s\n", key, cfg_get(config, key));
    return -1;
  }
  return 0;
}

int cfg_get_hex(struct cfg_struct *config, const char * key, short unsigned int *value)
{
  if(value == NULL)
  {
    printf("ERROR: value should be pointer\n");
    return -1;
  }
  const char *value_i = cfg_get(config, key);
  if(value_i == NULL)
  {
    printf("ERROR: could not find %s value in config file\n", key);
    return -1;
  }
  unsigned int temp_value;
  if(sscanf(value_i, "0x%x", &temp_value) != 1)
  {
    printf("ERROR: could not parse %s value: %s\n", key, cfg_get(config, key));
    return -1;
  }
  *value = (short unsigned int)temp_value;
  return 0;
}

int cfg_get_mac(struct cfg_struct *config, const char * key, unsigned char *MAC)
{
  if(MAC == NULL)
  {
    printf("ERROR: value should not be NULL\n");
    return -1;
  }
  const char *value_i = cfg_get(config, key);
  if(value_i == NULL)
  {
    printf("ERROR: could not find %s value in config file\n", key);
    return -1;
  }

  int values[6];
  if( 6 == sscanf( value_i, "\"%x:%x:%x:%x:%x:%x%*c[^\"]\"",
      &values[0], &values[1], &values[2],
      &values[3], &values[4], &values[5] ) )
  {
    int i;
      /* convert to uint8_t */
      for(i = 0; i < 6; ++i )
          MAC[i] = (unsigned char) values[i];
  }
  else
  {
      printf("ERROR: coud not parse mac. value: %s, please use format \"aa:bb:cc:dd:ee:ff\"\n",cfg_get(config, key));/* invalid mac */
      return -1;
  }
  return 0;
}