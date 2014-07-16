<?php
// $Id$

/**
 * @file
 * Encryption functionality for the Field encrypt module.
 */

/**
 * Add encryption to a field.
 *
 * Called when the "Encrypt this field" setting is first applied.
 */
function field_encrypt_do_encrypt($field_info) {

  $field_name = $field_info['field_name'];
  $data_table = 'field_data_' . $field_name;
  $revision_table = 'field_revision_' . $field_name;
  $value_field = $field_name . '_value';

  // Get all the current entries.
  $data = db_select($data_table, 't')->fields('t')->execute()->fetchAll();
  $revisions = db_select($revision_table, 't')->fields('t')->execute()->fetchAll();

  // Delete the existing unencrypted data.
  db_delete($data_table)->execute();
  db_delete($revision_table)->execute();

  // Make the field encryptable.
  make_field_encryptable($field_info);

  // Encrypt and re-insert the data.
  foreach($data as $k => $record) {
    $record->{$value_field} = field_encrypt_encrypt(serialize($record->{$value_field}));
    drupal_write_record($data_table, $record);
  }

  // Encrypt and re-insert the revisions.
  foreach($revisions as $k => $record) {
    $record->{$value_field} = field_encrypt_encrypt(serialize($record->{$value_field}));
    drupal_write_record($revision_table, $record);
  }

  drupal_set_message(t('%field_name is now being encrypted', array('%field_name' => $field_name)));
}

/**
 * Remove encryption from a field.
 *
 * Called when the "Encrypt this field" setting is disabled.
 */
function field_encrypt_un_encrypt($field_info) {

  $field_name = $field_info['field_name'];
  $data_table = 'field_data_' . $field_name;
  $revision_table = 'field_revision_' . $field_name;
  $value_field = $field_name . '_value';

  // Get all the current entries.
  $data = db_select($data_table, 't')->fields('t')->execute()->fetchAll();
  $revisions = db_select($revision_table, 't')->fields('t')->execute()->fetchAll();

  // Delete the existing encrypted data.
  db_delete($data_table)->execute();
  db_delete($revision_table)->execute();

  // Set the field back to un-encryptable.
  make_field_unencryptable($field_info);

  // Decrypt and re-insert the data.
  foreach($data as $k => $record) {
    $value = unserialize(field_encrypt_decrypt($record->{$value_field}));
	if(is_array($value)) {
		$value = $value[key($value)];
	}
    $record->{$value_field} = $value;
    drupal_write_record($data_table, $record);
  }

  // Encrypt and re-insert the revisions.
  foreach($revisions as $k => $record) {
  	$value = unserialize(field_encrypt_decrypt($record->{$value_field}));
	if(is_array($value)) {
	  $value = $value[key($value)];		
	}
    $record->{$value_field} = $value;
    drupal_write_record($revision_table, $record);
  }

  drupal_set_message(t('%field_name is no longer being encrypted', array('%field_name' => $field_name)));
}

/**
 * Implements hook_field_storage_pre_insert().
 */
function _field_encrypt_field_storage_pre_insert($entity_type, $entity, &$skip_fields) {
  _field_encrypt_field_storage_pre_update($entity_type, $entity, $skip_fields);
}

/**
 * Implements hook_field_storage_pre_update().
 */
function _field_encrypt_field_storage_pre_update($entity_type, $entity, &$skip_fields) {
  static $field_info = NULL;
  if ($field_info===NULL) {
    $field_info = field_info_field_by_ids();
  }

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

  $merge_keys = array(
    'entity_type' => $entity_type,
    'deleted' => 0,
    'entity_id' => $id,
  );

  foreach ($instances as $instance) {
    // Get the field data
    $field_id = $instance['field_id'];
    $field = $field_info[$field_id];

    // Are we encrypting this field?
    if (!isset($field['settings']['field_encrypt']['encrypt'])) {
      // encryption setting does not exist, skip it
      continue;
    }
    if (!$field['settings']['field_encrypt']['encrypt']) {
      // encryption setting is set to FALSE, skip it
      continue;
    }
    $field_name = $instance['field_name'];
	$data_table = 'field_data_' . $field_name;
	$revision_table = 'field_revision_' . $field_name;
	$value_field = $field_name . '_value';

    // We're bypassing usual storage, mark field for skipping
    $skip_fields[$field_id] = $field;

    // If there's nothing in the field, go no further
    if (empty($entity->$field_name)) {
      continue;
    }

    // Now we need to insert the data, we can't do multiple
    // merges so we have to do each language/delta separately.
    foreach ($entity->$field_name as $language => $items) {
      $merge_keys['language'] = $language;
      foreach ($items as $delta => $item) {
        $merge_keys['delta'] = $delta;
        $value = array();
        foreach ($item as $key => $val) {
          $value["{$field_name}_$key"] = $val;
        }
		
		$fields = array(
			$value_field => field_encrypt_encrypt(serialize($value)),  // encrypt here 
			'revision_id' => $vid,
		);
		// Put data in data table.
        db_merge($data_table)
          ->key($merge_keys)
          ->fields($fields)
          ->execute();
		  
		// Put data in revision table.
		$merge_keys['revision_id'] = $vid;
		unset($fields['revision_id']);
		db_merge($revision_table)
          ->key($merge_keys)
          ->fields($fields)
          ->execute();
      }
    }
  }
}

/**
 * Implements hook_field_storage_pre_load().
 */
function _field_encrypt_field_storage_pre_load($entity_type, $queried_entities, $age, &$skip_fields, $options) {
  static $field_info = NULL;
  if ($field_info===NULL) {
    $field_info = field_info_field_by_ids();
  }

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
      // Get the field data
      $field_id = $instance['field_id'];
      $field = $field_info[$field_id];

      // Are we encrypting this field?
      if (!isset($field['settings']['field_encrypt']['encrypt'])) {
        continue;
      }
      if (!$field['settings']['field_encrypt']['encrypt']) {
        continue;
      }
      $field_name = $instance['field_name'];
	  $revision_table = 'field_revision_' . $field_name;
	  $value_field = $field_name . '_value';

      // We're bypassing usual storage, mark field for skipping
      $skip_fields[$field_id] = $field;

      // Now we need to fetch the encrypted data		
	  $results = db_select($revision_table, 'fe', array('fetch' => PDO::FETCH_ASSOC))
        ->fields('fe', array('language', 'delta', $value_field))
        ->condition('fe.entity_type', $entity_type)
        ->condition('fe.entity_id', $id)
        ->condition('fe.revision_id', $vid)
        ->orderBy('fe.language', 'ASC')
        ->orderBy('fe.delta', 'ASC')
        ->execute();	
		
      $field_data = array();
      $index = strlen($field_name) + 1;
      foreach ($results as $result) {
      	
        extract($result); // decrypt here
        $value = field_encrypt_decrypt($$value_field);
        $item = array();
        foreach ((array) unserialize($value) as $key => $val) {
          $item[substr($key, $index)] = $val;
        }
        $field_data[$language][$delta] = $item;
      }
      $entity->$field_name = $field_data;
    }
  }
}

/**
 * Encrypt raw message
 */
function field_encrypt_encrypt($raw) {
  $encrypted = encrypt($raw, array('base64' => TRUE));
  return utf8_encode($encrypted);
}

/**
 * Decrypt encrypted message
 */
function field_encrypt_decrypt($encrypted) {
  $encrypted = utf8_decode($encrypted);
  $decrypted = decrypt($encrypted, array('base64' => TRUE));
  return $decrypted;
}

/**
 * Alter the data and revision table to allow storing of encrypted data.
 */
function make_field_encryptable($field_info = NULL) {
  if (!$field_info) {
    return FALSE;
  }

  $field_name = $field_info['field_name'];
  $data_table = 'field_data_' . $field_name;
  $revision_table = 'field_revision_' . $field_name;
  $value_field = $field_name . '_value';

  $spec = array(
    'type' => 'text',
    'size' => 'medium',
    'description' => 'The encrypted value for this entity field language/delta.',
  );

  db_change_field($data_table, $value_field, $value_field, $spec);
  db_change_field($revision_table, $value_field, $value_field, $spec);

}

/**
 * Alter the data and revision table to disallow storing of encrypted data.
 */
function make_field_unencryptable($field_info = NULL) {
  if (!$field_info) {
    return FALSE;
  }

  $field_name = $field_info['field_name'];
  $data_table = 'field_data_' . $field_name;
  $revision_table = 'field_revision_' . $field_name;
  $value_field = $field_name . '_value';

  $spec = array(
    'type' => 'varchar',
    'length' => 255,
    'description' => '',
  );

  db_change_field($data_table, $value_field, $value_field, $spec);
  db_change_field($revision_table, $value_field, $value_field, $spec);

}