<?php
/**
 * @file
 * Utility functions for use with the Islandora Simple Workflow module.
 */

/**
 * Query for inactive objects.
 *
 * @return array
 *   An array containing the results of the inactive objects query.
 */
function islandora_simple_workflow_get_inactive_objects() {
  $connection = islandora_get_tuque_connection();
  $query = <<<EOQ
SELECT ?object ?title FROM <#ri> WHERE
{
  ?object <fedora-model:label> ?title ;
          <fedora-model:state> <info:fedora/fedora-system:def/model#Inactive>
  !optionals
  !filters
}
ORDER BY ?object
EOQ;
  $optionals = (array) module_invoke('islandora_xacml_api', 'islandora_basic_collection_get_query_optionals', 'view');
  $filter_modules = array(
    'islandora_xacml_api',
    'islandora',
  );
  $filters = array();
  foreach ($filter_modules as $module) {
    $filters = array_merge_recursive($filters, (array) module_invoke($module, 'islandora_basic_collection_get_query_filters', 'view'));
  }
  $filter_map = function ($filter) {
    return "FILTER($filter)";
  };
  $sparql_query = format_string($query, array(
    '!optionals' => !empty($optionals) ? ('OPTIONAL {{' . implode('} UNION {', $optionals) . '}}') : '',
    '!filters' => !empty($filters) ? implode(' ', array_map($filter_map, $filters)) : '',
  ));
  $results = $connection->repository->ri->sparqlQuery($sparql_query);
  return $results;
}

/**
 * Query to determine if object belongs to site.
 *
 * @return boolean
 *   Boolean set to true if object belongs to site.
 */
function islandora_simple_workflow_belongs_to_site($query_pid) {

    $connection = islandora_get_tuque_connection();
    $root_pid = variable_get('islandora_repository_pid', 'islandora:root');

    // check for isComponentOf
    $component_of = array();
    $query_object = islandora_object_load($query_pid);
    if ((isset($query_object))&&($query_object != FALSE)) {
      $component_of = $query_object->relationships->get(ISLANDORA_RELS_EXT_URI, 'isComponentOf');
      if (count($component_of) > 0) {
          $query_pid = $component_of[0]['object']['value'];
      }
    }
    
    // check for isMemberOf
    $member_of = array();
    $query_object = islandora_object_load($query_pid);
    if ((isset($query_object))&&($query_object != FALSE)) {
      $member_of = $query_object->relationships->get(ISLANDORA_RELS_EXT_URI, 'isMemberOf');
      if (count($member_of) > 0) {
        // get parent object that is a member of a collection
        $parent_query = <<<'EOQ'
SELECT ?parentObject ?collection
FROM <#ri>
WHERE {
  ?parentObject <fedora-rels-ext:isMemberOfCollection> ?collection .
  <info:fedora/!pid> <fedora-rels-ext:isMemberOf>+ ?parentObject .
}
EOQ;
        $formatted_parent_query = format_string($parent_query, array(
          '!pid' => $query_pid,
        ));
        $results = $connection->repository->ri->sparqlQuery($formatted_parent_query, 'unlimited');
        if (count($results) > 0) {
            $query_pid = $results[0]['parentObject']['value'];
        }
      }
    }
    
    // query for parent collection with pid matching site root
    $collection_query = <<<'EOQ'
SELECT ?collection
FROM <#ri>
WHERE {
  <info:fedora/!pid> <fedora-rels-ext:isMemberOfCollection>+ ?collection
  FILTER(regex(str(?collection), "info:fedora/!root"))
}
EOQ;
    $formatted_collection_query = format_string($collection_query, array(
      '!pid' => $query_pid,
      '!root' => $root_pid,
    ));
    $results = $connection->repository->ri->sparqlQuery($formatted_collection_query, 'unlimited');
    if (count($results) > 0) {
        return true;
    }
    
    return false;
}
