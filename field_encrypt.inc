<?php

/**
 * @file
 * Encryption functionality for the Field encrypt module.
 */

/**
 * Add encryption to a field.
 *
 * Called when the "Encrypt this field" setting is first applied.
 *
 * @param array $field_info
 *   The field definition.
 * @param bool $has_data
 *   Whether the field has data.
 */
function field_encrypt_do_encrypt($field_info, $has_data) {
  // Column spec for encrypted data.
  $spec = array(
    'type' => 'text',
    'size' => 'medium',
    'description' => 'The encrypted value for this entity field column.',
  );

  foreach (array(FIELD_LOAD_CURRENT, FIELD_LOAD_REVISION) as $age) {
    if (!isset($field_info['storage']['details']['sql'][$age]) || !is_array($field_info['storage']['details']['sql'][$age])) {
      continue;
    }
    $table = key($field_info['storage']['details']['sql'][$age]);

    if ($has_data) {
      // Get all the current entries.
      $data = db_select($table, 't')->fields('t')->execute()->fetchAll();

      // Delete the existing unencrypted data.
      db_delete($table)->execute();
    }

    // Alter field columns to allow for encrypted data.
    foreach ($field_info['storage']['details']['sql'][$age][$table] as $column => $field) {
      if (!isset($field_info['indexes'][$column])) {
        db_change_field($table, $field, $field, $spec);
      }
    }

    // Encrypt and re-insert the data.
    if ($has_data && isset($data)) {
      foreach ($data as $record) {
        $query = db_insert($table);
        foreach ($field_info['storage']['details']['sql'][$age][$table] as $column => $field) {
          // Don't re-write empty, NULL values.
          if (empty($record->{$field})) {
            unset($record->{$field});
          }

          // Don't encrypt indexes.
          if (isset($field_info['indexes'][$column])) {
            continue;
          }

          if (isset($record->{$field})) {
            $record->{$field} = field_encrypt_encrypt($record->{$field});
          }
        }
        $query->fields((array) $record)->execute();
      }
    }
  }

  backdrop_set_message(t('%field_name is now being encrypted', array('%field_name' => $field_info['field_name'])));
}

/**
 * Remove encryption from a field.
 *
 * Called when the "Encrypt this field" setting is disabled.
 *
 * @param array $field_info
 *   The field definition.
 * @param bool $has_data
 *   Whether the field has data.
 */
function field_encrypt_un_encrypt($field_info, $has_data) {
  foreach (array(FIELD_LOAD_CURRENT, FIELD_LOAD_REVISION) as $age) {
    $table = key($field_info['storage']['details']['sql'][$age]);

    if ($has_data) {
      // Get all the current entries.
      $data = db_select($table, 't')->fields('t')->execute()->fetchAll();

      // Delete the existing encrypted data.
      db_delete($table)->execute();
    }

    // Revert the field columns back to their original state.
    foreach ($field_info['storage']['details']['sql'][$age][$table] as $column => $field) {
      if (!isset($field_info['indexes'][$column])) {
        db_change_field($table, $field, $field, $field_info['columns'][$column]);
      }
    }

    // Decrypt and re-insert the data.
    if ($has_data && isset($data)) {
      foreach ($data as $record) {
        $query = db_insert($table);
        foreach ($field_info['storage']['details']['sql'][$age][$table] as $column => $field) {
          // Don't re-write empty or NULL values.
          if (empty($record->{$field})) {
            unset($record->{$field});
          }

          // Don't un-encrypt indexes.
          if (isset($field_info['indexes'][$column])) {
            continue;
          }

          if (isset($record->{$field})) {
            $record->{$field} = field_encrypt_decrypt($record->{$field});
          }
        }
        $query->fields((array) $record)->execute();
      }
    }
  }

  backdrop_set_message(t('%field_name is no longer being encrypted', array('%field_name' => $field_info['field_name'])));
}

/**
 * Implements hook_field_storage_pre_update().
 */
function _field_encrypt_field_storage_pre_update($entity_type, $entity, &$skip_fields) {
  list($id, $vid, $bundle) = entity_extract_ids($entity_type, $entity);
  if (is_null($vid)) {
    $vid = $id;
  }
  $default_options = array(
    'default' => FALSE,
    'deleted' => FALSE,
    'language' => NULL,
  );
  $instances = _field_invoke_get_instances($entity_type, $bundle, $default_options);

  foreach ($instances as $instance) {
    // Get the field data.
    $field_name = $instance['field_name'];
    $current_field_info = field_info_field($field_name);

    // Are we encrypting this field?
    if (!isset($current_field_info['settings']['field_encrypt']['encrypt']) || !$current_field_info['settings']['field_encrypt']['encrypt']) {
      // Encryption setting is set to FALSE, skip it.
      continue;
    }

    $field_name = $instance['field_name'];
    $data_table = key($current_field_info['storage']['details']['sql'][FIELD_LOAD_CURRENT]);
    $revision_table = key($current_field_info['storage']['details']['sql'][FIELD_LOAD_REVISION]);

    // We're bypassing usual storage, mark field for skipping.
    $skip_fields[$field_name] = $current_field_info;

    // If there's nothing in the field, go no further.
    if (empty($entity->{$field_name})) {
      continue;
    }

    // Now we need to insert the data, we can't do multiple merges so we have to
    // do each language/delta separately.
    foreach ($entity->{$field_name} as $language => $items) {
      // Delete and Insert, like field_sql_storage_field_storage_write() does.
      db_delete($data_table)
        ->condition('entity_id', $id)
        ->condition('entity_type', $entity_type)
        ->condition('revision_id', $vid)
        ->execute();

      db_delete($revision_table)
        ->condition('entity_id', $id)
        ->condition('entity_type', $entity_type)
        ->condition('revision_id', $vid)
        ->execute();

      foreach ($items as $delta => $item) {
        $merge_keys = array(
          'entity_type' => $entity_type,
          'bundle' => $bundle,
          'deleted' => 0,
          'entity_id' => $id,
          'language' => $language,
          'delta' => $delta,
        );

        $fields = array(
          'revision_id' => $vid,
        );
        foreach ($current_field_info['storage']['details']['sql'][FIELD_LOAD_CURRENT][$data_table] as $column => $field) {
          if (isset($item[$column]) && !empty($item[$column])) {
            $fields[$field] = !isset($current_field_info['indexes'][$column])?field_encrypt_encrypt($item[$column]) : $item[$column];
          }
        }

        // Put data in data table.
        db_merge($data_table)->key($merge_keys)->fields($fields)->execute();

        // Put data in revision table.
        $merge_keys['revision_id'] = $vid;
        unset($fields['revision_id']);
        if (!empty($fields)) {
          db_merge($revision_table)
            ->key($merge_keys)
            ->fields($fields)
            ->execute();
        }
      }
    }
  }
}

/**
 * Implements hook_field_storage_pre_load().
 */
function _field_encrypt_field_storage_pre_load($entity_type, $queried_entities, $age, &$skip_fields, $options) {
  foreach ($queried_entities as $entity) {
    list($id, $vid, $bundle) = entity_extract_ids($entity_type, $entity);
    if (is_null($vid)) {
      $vid = $id;
    }
    $default_options = array(
      'default' => FALSE,
      'deleted' => FALSE,
      'language' => NULL,
    );
    $instances = _field_invoke_get_instances($entity_type, $bundle, $default_options);

    foreach ($instances as $instance) {
      // Get the field data.
      $field_name = $instance['field_name'];
      $current_field_info = field_info_field($field_name);

      // Are we encrypting this field?
      if (!isset($current_field_info['settings']['field_encrypt']['encrypt']) || !$current_field_info['settings']['field_encrypt']['encrypt']) {
        continue;
      }

      $field_name = $instance['field_name'];

      // Get the CURRENT or REVISION field table.
      $table = key($current_field_info['storage']['details']['sql'][$age]);

      // We're bypassing usual storage, mark field for skipping.
      $skip_fields[$field_name] = $current_field_info;

      // Now we need to fetch the encrypted data.
      $results = db_select($table, 'r')->fields('r')
        ->condition('r.entity_type', $entity_type)
        ->condition('r.entity_id', $id)->condition('r.revision_id', $vid)
        ->orderBy('r.language', 'ASC')->orderBy('r.delta', 'ASC')->execute()
        ->fetchAll();

      $field_data = array();
      foreach ($results as $result) {
        foreach ($current_field_info['storage']['details']['sql'][$age][$table] as $column => $field) {
          if (isset($result->{$field}) && !empty($result->{$field})) {
            $field_data[$result->language][$result->delta][$column] = !isset($current_field_info['indexes'][$column])?field_encrypt_decrypt($result->{$field}) : $result->{$field};
          }
        }
      }
      $entity->{$field_name} = $field_data;
    }
  }
}

/**
 * Encrypt raw message.
 *
 * @param string $raw
 *   The raw message to be encrypted.
 *
 * @return string
 *   The encrypted data.
 */
function field_encrypt_encrypt($raw) {
  try {
    $encrypted = encrypt($raw, array('base64' => TRUE));
  }
  catch (Exception $e) {
    return null;
  }

  return mb_convert_encoding($encrypted, 'UTF-8', 'ISO-8859-1');
}

/**
 * Decrypt encrypted message.
 *
 * @param string $encrypted
 *   The encrypted data.
 *
 * @return mixed|string
 *   The raw message.
 */
function field_encrypt_decrypt($encrypted) {
  $encrypted = mb_convert_encoding($encrypted, 'ISO-8859-1', 'UTF-8');
  try {
    $decrypted = decrypt($encrypted, array('base64' => TRUE));
  }
  catch (Exception $e) {
    return null;
  }

  return $decrypted;
}
