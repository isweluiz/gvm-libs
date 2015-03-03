/* openvas-libraries/base
 * $Id$
 * Description: Implementation of OpenVAS Networking related API.
 *
 * Authors:
 * Hani Benhabiles <hani.benhabiles@greenbone.net>
 *
 * Copyright:
 * Copyright (C) 2013 Greenbone Networks GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "openvas_networking.h"

#include <fcntl.h>

#include <libssh/libssh.h>
#include <gnutls/x509.h>

 /* Global variables */

/* Source interface name eg. eth1. */
char global_source_iface[IFNAMSIZ] = { '\0' };

/* Source IPv4 address. */
struct in_addr global_source_addr = { .s_addr = 0 };

/* Source IPv6 address. */
struct in6_addr global_source_addr6 = { .s6_addr32 = { 0, 0, 0, 0 } };

 /* Source Interface/Address related functions. */

/**
 * @brief Initializes the source network interface name and related information.
 *
 * @param[in]  iface    Name of network interface to use as source interface.
 *
 * @return 0 if success. If error, return 1 and reset source values to default.
 */
int
openvas_source_iface_init (const char *iface)
{
  struct ifaddrs *ifaddr, *ifa;
  int ret = 1;

  bzero (global_source_iface, sizeof (global_source_iface));
  global_source_addr.s_addr = INADDR_ANY;
  global_source_addr6 = in6addr_any;

  if (iface == NULL)
    return ret;

  if (getifaddrs (&ifaddr) == -1)
    return ret;

  /* Search for the adequate interface/family. */
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr && strcmp (iface, ifa->ifa_name) == 0)
        {
          if (ifa->ifa_addr->sa_family == AF_INET)
            {
              struct in_addr *addr = &((struct sockaddr_in *)
                                       ifa->ifa_addr)->sin_addr;

              memcpy (&global_source_addr, addr, sizeof (global_source_addr));
              ret = 0;
            }
          else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
              struct sockaddr_in6 *addr;

              addr = (struct sockaddr_in6 *) ifa->ifa_addr;
              memcpy (&global_source_addr6.s6_addr, &addr->sin6_addr,
                      sizeof (struct in6_addr));
              ret = 0;
            }
        }
    }

  /* At least one address for the interface was found. */
  if (ret == 0)
    strncpy (global_source_iface, iface, sizeof (global_source_iface));

  freeifaddrs (ifaddr);
  return ret;
}

int
openvas_source_iface_is_set (void)
{
  return *global_source_iface != '\0';
}

/**
 * @brief Binds a socket to use the global source address.
 *
 * @param[in]  socket    Socket to set source address for.
 * @param[in]  port      Network port for socket.
 * @param[in]  family    Family of socket. AF_INET or AF_INET6.
 *
 * @return 0 if success, -1 if error.
 */
int
openvas_source_set_socket (int socket, int port, int family)
{
  if (family == AF_INET)
    {
      struct sockaddr_in addr;

      openvas_source_addr (&addr.sin_addr);
      addr.sin_port = htons (port);
      addr.sin_family = AF_INET;

      if (bind (socket, (struct sockaddr *) &addr, sizeof (addr)) < 0)
        return -1;
    }
  else if (family == AF_INET6)
    {
      struct sockaddr_in6 addr6;

      openvas_source_addr6 (&addr6.sin6_addr);
      addr6.sin6_port = htons (port);
      addr6.sin6_family = AF_INET6;

      if (bind (socket, (struct sockaddr *) &addr6, sizeof (addr6)) < 0)
        return -1;
    }
  else
    return -1;

  return 0;
}

/**
 * @brief Gives the source IPv4 address.
 *
 * @param[out]  addr  Buffer of at least 4 bytes.
 */
void
openvas_source_addr (void *addr)
{
  if (addr)
    memcpy (addr, &global_source_addr.s_addr, 4);
}

/**
 * @brief Gives the source IPv6 address.
 *
 * @param[out]  addr6  Buffer of at least 16 bytes.
 */
void
openvas_source_addr6 (void *addr6)
{
  if (addr6)
    memcpy (addr6, &global_source_addr6.s6_addr, 16);
}

/**
 * @brief Gives the source IPv4 mapped as an IPv6 address.
 * eg. 192.168.20.10 would map to ::ffff:192.168.20.10.
 *
 * @param[out]  addr6  Buffer of at least 16 bytes.
 */
void
openvas_source_addr_as_addr6 (struct in6_addr *addr6)
{
  if (addr6)
    ipv4_as_ipv6 (&global_source_addr, addr6);
}

/**
 * @brief Gives the source IPv4 address in string format.
 *
 * @return Source IPv4 string. Free with g_free().
 */
char *
openvas_source_addr_str (void)
{
  char *str = g_malloc0 (INET_ADDRSTRLEN);

  inet_ntop (AF_INET, &global_source_addr.s_addr, str, INET_ADDRSTRLEN);
  return str;
}

/**
 * @brief Gives the source IPv6 address in string format.
 *
 * @return Source IPv6 string. Free with g_free().
 */
char *
openvas_source_addr6_str (void)
{
  char *str = g_malloc0 (INET6_ADDRSTRLEN);

  inet_ntop (AF_INET6, &global_source_addr6, str, INET6_ADDRSTRLEN);
  return str;
}

 /* Miscellaneous functions. */

/**
 * @brief Maps an IPv4 address as an IPv6 address.
 * eg. 192.168.10.20 would map to ::ffff:192.168.10.20.
 *
 * @param[in]  ip4  IPv4 address to map.
 * @param[out] ip6  Buffer to store the IPv6 address.
 */
void
ipv4_as_ipv6 (const struct in_addr *ip4, struct in6_addr *ip6)
{
  if (ip4 == NULL || ip6 == NULL)
    return;

  ip6->s6_addr32[0] = 0;
  ip6->s6_addr32[1] = 0;
  ip6->s6_addr32[2] = htonl (0xffff);
  memcpy (&ip6->s6_addr32[3], ip4, sizeof (struct in_addr));
}

char *
addr6_as_str (const struct in6_addr *addr6)
{
  char *str;

  if (!addr6)
    return NULL;

  str = g_malloc0 (INET6_ADDRSTRLEN);
  if (IN6_IS_ADDR_V4MAPPED (addr6))
    inet_ntop (AF_INET, &addr6->s6_addr32[3], str, INET6_ADDRSTRLEN);
  else
    inet_ntop (AF_INET6, addr6, str, INET6_ADDRSTRLEN);
  return str;
}

/**
 * @brief Resolves a hostname to an IPv4 or IPv6 address.
 *
 * @param[in]   name    Hostname to resolve.
 * @param[out]  dst     Buffer to store resolved address. Size must be at least
 *                      4 bytes for AF_INET and 16 bytes for AF_INET6.
 * @param[in] family    Either AF_INET or AF_INET6.
 *
 * @return -1 if error, 0 otherwise.
 */
int
openvas_resolve (const char *name, void *dst, int family)
{
  struct addrinfo hints, *info, *p;

  if (name == NULL || dst == NULL
      || (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC))
    return -1;

  bzero (&hints, sizeof (hints));
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  if ((getaddrinfo (name, NULL, &hints, &info)) != 0)
    return -1;

  p = info;
  while (p)
    {
      if (p->ai_family == family || family == AF_UNSPEC)
        {
          if (p->ai_family == AF_INET && family == AF_UNSPEC)
            {
              struct sockaddr_in *addrin = (struct sockaddr_in *) p->ai_addr;
              ipv4_as_ipv6 (&(addrin->sin_addr), dst);
            }
          else if (p->ai_family == AF_INET)
            {
              struct sockaddr_in *addrin = (struct sockaddr_in *) p->ai_addr;
              memcpy (dst, &(addrin->sin_addr), sizeof (struct in_addr));
            }
          else if (p->ai_family == AF_INET6)
            {
              struct sockaddr_in6 *addrin = (struct sockaddr_in6 *) p->ai_addr;
              memcpy (dst, &(addrin->sin6_addr), sizeof (struct in6_addr));
            }
          break;
        }

      p = p->ai_next;
    }

  freeaddrinfo (info);
  return 0;
}

/**
 * @brief Resolves a hostname to an IPv4-mapped IPv6 or IPv6 address.
 *
 * @param[in]   name    Hostname to resolve.
 * @param[out]  ip6     Buffer to store resolved address.
 *
 * @return -1 if error, 0 otherwise.
 */
int
openvas_resolve_as_addr6 (const char *name, struct in6_addr *ip6)
{
  return openvas_resolve (name, ip6, AF_UNSPEC);
}

/* Ports related. */

/**
 * @brief Validate a port range string.
 *
 * Accepts ranges in form of "103,U:200-1024,3000-4000,T:3-4,U:7".
 *
 * @param[in]   port_range  A port range.
 *
 * @return 0 success, 1 failed.
 */
int
validate_port_range (const char* port_range)
{
  gchar **split, **point, *range, *range_start;

  if (!port_range)
    return 1;

  while (*port_range && isblank (*port_range)) port_range++;
  if (*port_range == '\0')
    return 1;

  /* Treat newlines like commas. */
  range = range_start = g_strdup (port_range);
  while (*range)
    {
      if (*range == '\n') *range = ',';
      range++;
    }

  split = g_strsplit (range_start, ",", 0);
  g_free (range_start);
  point = split;

  while (*point)
    {
      gchar *hyphen, *element;

      /* Strip off any outer whitespace. */

      element = g_strstrip (*point);

      /* Strip off any leading type specifier. */

      if ((strlen (element) >= 2)
          && ((element[0] == 'T') || (element[0] == 'U'))
          && (element[1] == ':'))
        element = element + 2;

      /* Look for a hyphen. */

      hyphen = strchr (element, '-');
      if (hyphen)
        {
          long int number1, number2;
          const char *first;
          char *end;

          hyphen++;

          /* Check the first number. */

          first = element;
          while (*first && isblank (*first)) first++;
          if (*first == '-')
            goto fail;

          errno = 0;
          number1 = strtol (first, &end, 10);
          while (*end && isblank (*end)) end++;
          if (errno || (*end != '-'))
            goto fail;
          if (number1 == 0)
            goto fail;
          if (number1 > 65535)
            goto fail;

          /* Check the second number. */

          while (*hyphen && isblank (*hyphen)) hyphen++;
          if (*hyphen == '\0')
            goto fail;

          errno = 0;
          number2 = strtol (hyphen, &end, 10);
          while (*end && isblank (*end)) end++;
          if (errno || *end)
            goto fail;
          if (number2 == 0)
            goto fail;
          if (number2 > 65535)
            goto fail;

          if (number1 > number2)
            goto fail;
        }
      else
        {
          long int number;
          const char *only;
          char *end;

          /* Check the single number. */

          only = element;
          while (*only && isblank (*only)) only++;
          /* Empty ranges are OK. */
          if (*only)
            {
              errno = 0;
              number = strtol (only, &end, 10);
              while (*end && isblank (*end)) end++;
              if (errno || *end)
                goto fail;
              if (number == 0)
                goto fail;
              if (number > 65535)
                goto fail;
            }
        }
      point += 1;
    }

  g_strfreev (split);
  return 0;

 fail:
  g_strfreev (split);
  return 1;
}

/**
 * @brief Create a range array from a port_range string.
 *
 * @param[in]   port_range  Valid port_range string.
 *
 * @return Range array.
 */
array_t*
port_range_ranges (const char *port_range)
{
  gchar **split, **point, *range_start, *current;
  array_t *ranges;
  int tcp;

  ranges = make_array ();

  while (*port_range && isblank (*port_range)) port_range++;

  /* Accepts T: and U: before any of the ranges.  This toggles the remaining
   * ranges, as in nmap.  Treats a leading naked range as TCP, whereas nmap
   * treats it as TCP and UDP. */

  /* Treat newlines like commas. */
  range_start = current = g_strdup (port_range);
  while (*current)
    {
      if (*current == '\n') *current = ',';
      current++;
    }

  tcp = 1;
  split = g_strsplit (range_start, ",", 0);
  g_free (range_start);
  point = split;

  while (*point)
    {
      gchar *hyphen, *element;
      range_t *range;

      element = g_strstrip (*point);
      if (strlen (element) >= 2)
        {
          if ((element[0] == 'T') && (element[1] == ':'))
            {
              tcp = 1;
              element = element + 2;
            }
          else if ((element[0] == 'U') && (element[1] == ':'))
            {
              tcp = 0;
              element = element + 2;
            }
          /* Else tcp stays as it is. */
        }

      /* Skip any space that followed the type specifier. */
      while (*element && isblank (*element)) element++;

      hyphen = strchr (element, '-');
      if (hyphen)
        {
          *hyphen = '\0';
          hyphen++;
          while (*hyphen && isblank (*hyphen)) hyphen++;
          assert (*hyphen);  /* Validation checks this. */

          /* A range. */

          range = (range_t*) g_malloc0 (sizeof (range_t));

          range->start = atoi (element);
          range->end = atoi (hyphen);
          range->type = tcp ? PORT_PROTOCOL_TCP : PORT_PROTOCOL_UDP;
          range->exclude = 0;

          array_add (ranges, range);
        }
      else if (*element)
        {
          /* A single port. */

          range = (range_t*) g_malloc0 (sizeof (range_t));

          range->start = atoi (element);
          range->end = range->start;
          range->type = tcp ? PORT_PROTOCOL_TCP : PORT_PROTOCOL_UDP;
          range->exclude = 0;

          array_add (ranges, range);
        }
      /* Else skip over empty range. */
      point += 1;
    }
  g_strfreev (split);
  return ranges;
}

/**
 * @brief Checks if a port num is in port ranges array.
 *
 * @param[in]  pnum     Port number.
 * @param[in]  ptype    Port type.
 * @param[in]  pranges  Array of port ranges.
 *
 * @return 1 if port in port ranges, 0 otherwise.
 */
int
port_in_port_ranges (int pnum, port_protocol_t ptype, array_t *pranges)
{
  int i;

  if (pranges == NULL || pnum < 0 || pnum > 65536)
    return 0;

  for (i = 0; i < pranges->len; i++)
    {
      range_t *range = (range_t *) g_ptr_array_index (pranges, i);
      if (range->type != ptype)
        continue;
      if (range->start <= pnum && pnum <= range->end)
        return 1;
    }
  return 0;
}

/**
 * @brief Decrypts a base64 encrypted ssh private key.
 *
 * @param[in]   pkcs8_key       PKCS#8 encrypted private key.
 * @param[in]   passphrase      Passphrase for the private key.
 *
 * @return Decrypted private key if success, NULL otherwise.
 */
char *
openvas_ssh_pkcs8_decrypt (const char *pkcs8_key, const char *passphrase)
{
  gnutls_datum_t data;
  gnutls_x509_privkey_t key;
  char buffer[16 * 2048];
  int rc;
  size_t size = sizeof (buffer);

  rc = gnutls_x509_privkey_init (&key);
  if (rc)
    return NULL;
  data.size = strlen (pkcs8_key);
  data.data = (void *) g_strdup (pkcs8_key);
  rc = gnutls_x509_privkey_import_pkcs8 (key, &data, GNUTLS_X509_FMT_PEM,
                                         passphrase?: "", 0);
  if (rc)
    {
      gnutls_x509_privkey_deinit (key);
      return NULL;
    }
  g_free (data.data);
  rc = gnutls_x509_privkey_export (key, GNUTLS_X509_FMT_PEM, buffer, &size);
  gnutls_x509_privkey_deinit (key);
  if (rc)
    return NULL;
  return g_strdup (buffer);
}

/**
 * @brief Exports a base64 encoded public key from a private key and its
 *        passphrase.
 *
 * @param[in]   private_key     Private key to export.
 * @param[in]   passphrase      Passphrase for the private key.
 *
 * @return Allocated base64 encoded public key if success, NULL otherwise.
 */
char *
openvas_ssh_public_from_private (const char *private_key, const char *passphrase)
{
#if LIBSSH_VERSION_INT >= SSH_VERSION_INT (0, 6, 0)
  ssh_key priv;
  char *pub_key, *decrypted_priv, *pub_str = NULL;
  const char *type;
  int ret;

  decrypted_priv = openvas_ssh_pkcs8_decrypt (private_key, passphrase);
  ret = ssh_pki_import_privkey_base64
         (decrypted_priv ?: private_key, passphrase, NULL, NULL, &priv);
  g_free (decrypted_priv);
  if (ret)
    return NULL;
  ret = ssh_pki_export_pubkey_base64 (priv, &pub_key);
  type = ssh_key_type_to_char (ssh_key_type (priv));
  ssh_key_free (priv);
  if (ret)
    return NULL;
  pub_str = g_strdup_printf ("%s %s", type, pub_key);
  g_free (pub_key);
  return pub_str;

#else
  char key_dir[] = "/tmp/openvas_key_XXXXXX", *base64, *data;
  char filename[1024], *decrypted_priv;
  ssh_private_key ssh_privkey;
  ssh_public_key ssh_pubkey;
  ssh_session session;
  ssh_string sstring;
  size_t datalen;

  if (!private_key || !g_mkdtemp_full (key_dir, S_IRUSR|S_IWUSR|S_IXUSR))
    return NULL;
  g_snprintf (filename, sizeof (filename), "%s/key.tmp", key_dir);
  decrypted_priv = ssh_pkcs8_to_private (private_key, passphrase);
  if (!g_file_set_contents (filename, decrypted_priv ?: private_key, -1, NULL))
    {
      g_free (decrypted_priv);
      g_rmdir (key_dir);
      return NULL;
    }
  g_free (decrypted_priv);
  session = ssh_new ();
  ssh_privkey = privatekey_from_file (session, filename, 0, passphrase);
  ssh_free (session);
  g_remove (filename);
  g_rmdir (key_dir);
  if (!ssh_privkey)
    return NULL;
  /* Return as base64 encoded public key. */
  ssh_pubkey = publickey_from_privatekey (ssh_privkey);
  privatekey_free (ssh_privkey);
  if (!ssh_pubkey)
    return NULL;
  sstring = publickey_to_string (ssh_pubkey);
  publickey_free (ssh_pubkey);
  if (!sstring)
    return NULL;
  data = ssh_string_to_char (sstring);
  datalen = ssh_string_len (sstring);
  base64 = g_base64_encode ((guchar *) data, datalen);
  ssh_string_free (sstring);
  g_free (data);
  return base64;
#endif
}
