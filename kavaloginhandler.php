<?php

class KAVALoginHandler {

  const DEFAULT_LANGUAGE = 'nl_NL';

  private static $instance;
  private $customGroupCache = [];

  /**
   * Get instance
   * @return bool|\KAVALoginHandler Instance
   */
  public static function getInstance() {
    if (!self::$instance) {
      self::$instance = new self;
    }

    if (!civicrm_initialize()) {
      self::$instance->log('Could not initialize CiviCRM.', TRUE);
      return FALSE;
    }

    return self::$instance;
  }

  /**
   * Try to add a UFMatch record for a Drupal user by searching for his/her CAS username.
   * @param object $account Drupal user account
   * @return bool Success
   */
  public function match($account) {

    $match = $this->getUFMatch($account->uid);
    if ($match) {
      return TRUE; // UFMatch already exists
    }

    // Try to find CiviCRM contact by CAS login name
    $cas_username = cas_current_user();
    $contact = $this->getCiviContactByLoginName($cas_username);
    if (!$contact) {
      return FALSE; // No contact exists for this username
    }

    // Add match record
    $contact_id = $contact['contact_id'];
    $match = $this->addUFMatch($account->uid, $contact_id, $cas_username);
    if (!$match) {
      $this->log('Could not add UFMatch for user ' . $account->uid . ' with contact ' . $contact_id);
      return FALSE;
    }

    $this->log('Added UFMatch for user ' . $account->uid . ' / contact ' . $contact_id, FALSE, WATCHDOG_INFO);

    // Add CiviCRM email address to Drupal account
    if (isset($contact['email'])) {
      $account->mail = $contact['email'];
      $account->language = self::DEFAULT_LANGUAGE;
      user_save($account);
    }

    return TRUE;
  }

  /**
   * Try to get a CiviCRM contact by his/her CAS login name (custom field KAVA_account_login)
   * @param string $username Username
   * @return array|bool Contact|false
   */
  public function getCiviContactByLoginName($username) {

    try {
      $fieldId = $this->getCustomFieldId('contact_extra', 'KAVA_account_login');
      return civicrm_api3('Contact', 'getsingle', [
        'custom_' . $fieldId => $username,
        'return'             => 'contact_id,external_identifier,email,display_name',
      ]);
    } catch (\CiviCRM_API3_Exception $e) {
      $this->log('CiviCRM API error: ' . $e->getMessage());
      return FALSE;
    }
  }

  /**
   * Fetch UFMatch record
   * @param int $uf_id uf_id
   * @return array|null UFMatch record
   */
  private function getUFMatch($uf_id) {
    try {
      return civicrm_api3('UFMatch', 'getsingle', [
        'uf_id' => $uf_id,
      ]);
    } catch (\CiviCRM_API3_Exception $e) {
      return NULL;
    }
  }

  /**
   * Add UFMatch record
   * @param int $uf_id
   * @param int $contact_id
   * @param string $uf_name
   * @return bool Success
   * @throws \CiviCRM_API3_Exception Exception
   */
  private function addUFMatch($uf_id, $contact_id, $uf_name) {
    $ret = civicrm_api3('UFMatch', 'create', [
      'uf_id'      => $uf_id,
      'contact_id' => $contact_id,
      'uf_name'    => $uf_name,
      'language'   => self::DEFAULT_LANGUAGE,
    ]);
    return ($ret && !$ret['is_error']);
  }

  /**
   * Find a custom field ID by name
   * @param string $groupName CustomGroup name
   * @param string $fieldName CustomField name
   * @return int CustomField id
   * @throws \CiviCRM_API3_Exception Exception
   */
  private function getCustomFieldId($groupName, $fieldName) {

    $groupId = $this->getCustomGroupId($groupName);
    $fieldId = civicrm_api3('CustomField', 'getvalue', [
      'group_id' => $groupId,
      'name'     => $fieldName,
      'return'   => 'id',
    ]);
    return $fieldId;
  }

  /**
   * Find a custom group ID by name
   * @param string $groupName CustomGroup name
   * @return int CustomGroup id
   * @throws \CiviCRM_API3_Exception Exception
   */
  private function getCustomGroupId($groupName) {

    if (array_key_exists($groupName, $this->customGroupCache)) {
      return $this->customGroupCache[$groupName];
    };

    $groupId = civicrm_api3('CustomGroup', 'getvalue', [
      'name'   => $groupName,
      'return' => 'id',
    ]);
    $this->customGroupCache[$groupName] = $groupId;
    return $groupId;
  }

  /**
   * Log helper function (shortcut for watchdog / drupal_set_message)
   * @param string $message Message
   * @param bool|FALSE $display Display message
   * @param int $severity Severity (WATCHDOG_*)
   */
  private function log($message, $display = FALSE, $severity = WATCHDOG_WARNING) {
    if ($display) {
      drupal_set_message('KAVALogin: ' . $message, 'warning');
    }
    watchdog('kavalogin', $message, [], $severity);
  }

}