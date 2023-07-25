Field Encryption
================

This module provides a way to encrypt field values when stored in the database.

Installation
------------

Install this module using the official Backdrop CMS instructions at
<https://backdropcms.org/guide/modules>.

This module depends on the [Encrypt module](https://backdropcms.org/project/encrypt)
for encrypting and decrypting strings.

Configuration
-------------

This can be set in `settings.php` file by adding
`$settings['cache_class_cache_field'] = 'FieldEncryptDatabaseCache';`.

Field encryption settings are made on the field edit page.

Memcache support
----------------

To enable Memcache support for the Field Encryption module you have to ensure
that the Memcache Field Encryption Cache backend is enabled.

This can be set in `settings.php` file by adding
`$settings['cache_class_cache_field'] = 'FieldEncryptMemCacheBackdrop';`.

**Note:** Memcache support requires the [Memcache
module](https://backdropcms.org/project/memcache) is installed and configured.

License
-------

This project is GPL v2 software. See the LICENSE.txt file in this directory for
complete text.

Maintainers
-----------

* [Herb v/d Dool](https://github.com/herbdool)

Credits
-------

Ported to Backdrop by [Herb v/d Dool](https://github.com/herbdool).

Drupal version maintained by:

* [Caio Vitor Luppi de Paula (caiovlp)](https://www.drupal.org/u/caiovlp)
* [Stuart Clark (Deciphered)](https://www.drupal.org/u/deciphered)
* [Ted Bowman (tedbow)](https://www.drupal.org/u/tedbow)
