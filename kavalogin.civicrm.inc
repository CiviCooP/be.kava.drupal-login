<?php

/**
 * Class KAVALogin_CiviCRM
 * Functies om de loginfunctie te integreren met CiviCRM en een match met een contact te maken.
 */
class KAVALogin_CiviCRM {

  private static $instance;
  private $customGroupCache = [];
  private $customFieldsCache = [];

  /**
   * Get instance
   * @return bool|\KAVALogin_CiviCRM Instance
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
    if (!$cas_username) {
      return FALSE;
    }

    $contact = $this->getCiviContactByLoginName($cas_username);
    if (!$contact) {
      return FALSE; // No contact exists for this username
    }

    // Add match record
    $contact_id = $contact['contact_id'];
    $match = $this->addUFMatch($account->uid, $contact_id, $cas_username);
    if (!$match) {
      kavalogin_log('Could not add UFMatch for user ' . $account->uid . ' with contact ' . $contact_id);
      return FALSE;
    }

    kavalogin_log('Added UFMatch for user ' . $account->uid . ' / contact ' . $contact_id, FALSE, WATCHDOG_INFO);

    // Add CiviCRM email address to Drupal account
    if (isset($contact['email'])) {
      $account->mail = $contact['email'];
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

    return $this->getCiviContact('kava_account_login', $username);
  }

  /**
   * Try to get a CiviCRM contact by his/her barcode (custom field barcode
   * @param string $username Username
   * @return array|bool Contact|false
   */
  public function getCiviContactByBarcode($barcode) {

    return $this->getCiviContact('barcode', $barcode);
  }

  /**
   * Try to get a CiviCRM contact by his/her email address (for register form)
   * @param string $email Email
   * @return array|bool Contact|false
   */
  public function getCiviContactByEmail($email) {

    return $this->getCiviContact('email', $email);
  }

  /**
   * Return CiviContact by searching searchField for searchValue. Include KAVA account username.
   * @param string $searchField Search field
   * @param string $searchValue Search value
   * @param bool $extraFields Return kava_account_login and barcode?
   * @return array|bool Contact | false
   */
  private function getCiviContact($searchField, $searchValue, $extraFields = TRUE) {
    try {
      if ($extraFields) {
        $customFields = [
          'kava_account_login' => 'custom_' . $this->getCustomFieldId('contact_extra', 'KAVA_account_login'),
          'barcode'            => 'custom_' . $this->getCustomFieldId('contact_extra', 'barcode'),
        ];
        $searchField = (array_key_exists($searchField, $customFields) ? $customFields[$searchField] : $searchField);
      }
      else {
        $customFields = [];
      }

      $contact = civicrm_api3('Contact', 'getsingle', [
        $searchField         => $searchValue,
        'contact_is_deleted' => 0,
        'return'             => "contact_id,external_identifier,email,display_name" .
          ($extraFields ? ',' . implode(',', array_values($customFields)) : ''),
        'option.limit'       => 1,
      ]);

      if ($extraFields) {
        foreach ($customFields as $fieldName => $fieldCode) {
          $contact[$fieldName] = $contact[$fieldCode];
        }
      }

      return $contact;

    } catch (\CiviCRM_API3_Exception $e) {
      return FALSE;
    }
  }

  /**
   * Fetch UFMatch record
   * @param int $uf_id uf_id
   * @return array|bool UFMatch record or false
   */
  private function getUFMatch($uf_id) {
    try {
      return civicrm_api3('UFMatch', 'getsingle', [
        'uf_id' => $uf_id,
      ]);
    } catch (\CiviCRM_API3_Exception $e) {
      return FALSE;
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
  public function addUFMatch($uf_id, $contact_id, $uf_name) {

    kavalogin_log('Adding UFMatch record for contact id ' . $contact_id . ' and user id ' . $uf_id . ', uf_name is ' . $uf_name);

    try {
      $ret = civicrm_api3('UFMatch', 'create', [
        'uf_id'      => $uf_id,
        'contact_id' => $contact_id,
        'uf_name'    => $uf_name,
      ]);
      return ($ret && !$ret['is_error']);
    } catch (CiviCRM_API3_Exception $e) {
      kavalogin_log('CiviCRM API error: ' . $e->getMessage());
      return FALSE;
    }
  }

  /**
   * Check if an UFMatch record already exists for a contact
   * @param int $contact_id Contact ID
   * @return bool UFMatch Exists
   */
  public function checkUFMatch($contact_id) {

    try {
      $ret = civicrm_api3('UFMatch', 'getsingle', [
        'contact_id' => $contact_id,
      ]);
      return ($ret && !$ret['is_error']);
    } catch (CiviCRM_API3_Exception $e) {
      return FALSE;
    }
  }

  /**
   * Add (or update) contact record
   * @param array $params Parameters.
   * (Allowed params: contact_id, first_name, last_name, street_address, postal_code, city, country_id, phone, email, username, barcode)
   * @return bool Success
   * @throws \CiviCRM_API3_Exception Exception
   */
  public function addUpdateContact($params = []) {

    kavalogin_log('Adding/updating CiviCRM contact. Params: ' . print_r($params, TRUE), FALSE, WATCHDOG_INFO);

    try {

      $contactParams = [
        'contact_type' => 'Individual',
      ];

      foreach (['contact_id', 'first_name', 'last_name'] as $contactField) {
        if (!empty($params[$contactField])) {
          $contactParams[$contactField] = $params[$contactField];
        }
      }

      $usernameFieldName = 'custom_' . $this->getCustomFieldId('contact_extra', 'KAVA_account_login');
      $barcodeFieldName = 'custom_' . $this->getCustomFieldId('contact_extra', 'barcode');
      if (!empty($params['username'])) {
        $contactParams[$usernameFieldName] = $params['username'];
      }
      if (!empty($params['barcode'])) {
        $contactParams[$barcodeFieldName] = $params['barcode'];
      }

      if (!empty($params['street_address'])) {
        $contactParams['api.address.create'] = [
          'location_type_id' => 3,
          'is_primary'       => 1,
          'street_address'   => $params['street_address'],
          'postal_code'      => $params['postal_code'],
          'city'             => $params['city'],
          'country_id'       => (!empty($params['country_id']) ? $params['country_id'] : $this->getDefaultCountry()),
        ];
      }

      if (!empty($params['phone'])) {
        $contactParams['api.phone.create'] = [
          'location_type_id' => 3,
          'is_primary'       => 1,
          'phone'            => $params['phone'],
        ];
      }

      if (!empty($params['email'])) {
        $contactParams['api.email.create'] = [
          'is_primary' => 1,
          'email'      => $params['email'],
        ];
      }

      $ret = civicrm_api3('Contact', 'create', $contactParams);
      if ($ret && !$ret['is_error']) {
        return $ret['id'];
      }

      return FALSE;

    } catch (\CiviCRM_API3_Exception $e) {
      kavalogin_log('CiviCRM API error: ' . $e->getMessage());
      return FALSE;
    }
  }

  /**
   * Add a Drupal user. (Calls cas_user_register function.)
   * @param array $params Account params (username and email)
   * @return mixed|bool Account or false
   */
  public function addDrupalUser($params = []) {

    if (empty($params['username'])) {
      return FALSE;
    }

    kavalogin_log('Adding Drupal user with username ' . $params['username'], FALSE, WATCHDOG_INFO);

    // Trick to disable automatic UFMatching when adding a Drupal user
    $config = CRM_Core_Config::singleton();
    $config->inCiviCRM = TRUE;

    $account = cas_user_register($params['username']);
    $account->mail = $params['email'];
    $account = user_save($account);

    $config->inCiviCRM = FALSE;

    return $account;
  }

  /**
   * Get contact data field by Drupal user id
   * @param int $uid Drupal UID
   * @param string $fieldName Field name
   * @return mixed Field value or false
   */
  public function getFieldForDrupalUser($uid, $fieldName) {

    $ufmatch = $this->getUFMatch($uid);
    if (!$ufmatch) {
      return FALSE;
    }

    if ($fieldName == 'username') {
      $fieldName = 'custom_' . $this->getCustomFieldId('contact_extra', 'KAVA_account_login');
    }
    elseif ($fieldName == 'barcode') {
      $fieldName = 'custom_' . $this->getCustomFieldId('contact_extra', 'barcode');
    }

    try {
      return civicrm_api3('Contact', 'getvalue', [
        'contact_id' => $ufmatch['contact_id'],
        'return'     => $fieldName,
      ]);
    } catch (\CiviCRM_API3_Exception $e) {
      return FALSE;
    }
  }

  /**
   * Find a custom field ID by name
   * @param string $groupName CustomGroup name
   * @param string $fieldName CustomField name
   * @return int CustomField id
   * @throws \CiviCRM_API3_Exception Exception
   */
  private function getCustomFieldId($groupName, $fieldName) {

    $cacheKey = $groupName . '_' . $fieldName;
    if (array_key_exists($cacheKey, $this->customFieldsCache)) {
      return $this->customFieldsCache[$cacheKey];
    }

    $groupId = $this->getCustomGroupId($groupName);
    $fieldId = civicrm_api3('CustomField', 'getvalue', [
      'group_id' => $groupId,
      'name'     => $fieldName,
      'return'   => 'id',
    ]);

    $this->customFieldsCache[$cacheKey] = $fieldId;
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
   * Get country list from CiviCRM
   * @return array API result
   * @throws \CiviCRM_API3_Exception Cannot get country list
   */
  public function getCountries() {

    $countries = civicrm_api3('Address', 'Getoptions', [
      'field' => 'country_id',
    ]);
    return $countries['values'];
  }

  /**
   * Get default country ID from CiviCRM
   * @return int Country ID
   */
  public function getDefaultCountry() {
    return (int) CRM_Core_Config::singleton()->defaultContactCountry;
  }

}