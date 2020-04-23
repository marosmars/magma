/**
 * Copyright 2004-present Facebook. All Rights Reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @flow
 * @format
 */

import logging from '@fbcnms/logging';
import streamify from 'stream-array';
import {JSONPath} from 'jsonpath-plus';

const logger = logging.getLogger(module);

// Global prefix for taskdefs which can be used by all tenants.
// TODO: can we come up with an invalid tenant name?
export const GLOBAL_PREFIX = 'GLOBAL';

// This is used to separate tenant id from name in workflowdefs and taskdefs
export const INFIX_SEPARATOR = '___';

const SYSTEM_TASK_TYPES = [
  'DECISION',
  'EVENT',
  'HTTP',
  'SUB_WORKFLOW',
  'FORK',
  'FORK_JOIN_DYNAMIC',
  'JOIN',
  'EXCLUSIVE_JOIN',
  'WAIT',
  'DYNAMIC',
  'LAMBDA',
  'TERMINATE',
  'KAFKA_PUBLISH',
  'DO_WHILE',
];

function isAllowedSystemTask(task) {
  return SYSTEM_TASK_TYPES.includes(task.type);
}

export function assertAllowedSystemTask(task) {
  if (!isAllowedSystemTask(task)) {
    logger.error(
      `Task type is not allowed: ` + ` in '${JSON.stringify(task)}'`,
    );
    // TODO create Exception class
    throw 'Task type is not allowed';
  }
}

export function withUnderscore(s) {
  return s + INFIX_SEPARATOR;
}

export function addTenantIdPrefix(tenantId, objectWithName) {
  const tenantWithUnderscore = withUnderscore(tenantId);
  objectWithName.name =
    tenantWithUnderscore +
    assertNameIsWithoutInfixSeparator(objectWithName).name;
}

export function assertNameIsWithoutInfixSeparator(objectWithName) {
  if (objectWithName.name.indexOf(INFIX_SEPARATOR) > -1) {
    logger.error(
      `Name must not contain '${INFIX_SEPARATOR}'` +
        ` in '${JSON.stringify(objectWithName)}'`,
    );
    // TODO create Exception class
    throw 'Name must not contain underscore';
  }
}

export function getTenantId(req) {
  const tenantId = req.headers['x-auth-organization'];
  if (tenantId == null) {
    logger.error('x-auth-organization header not found');
    throw 'x-auth-organization header not found';
  }
  if (tenantId == GLOBAL_PREFIX) {
    logger.error(`Illegal name for TenantId: '${tenantId}'`);
    throw 'Illegal TenantId';
  }
  return tenantId;
}

// TODO: deprecated, use removeTenantPrefix
export function removeTenantId(json, attr, tenantId) {
  removeTenantPrefix(tenantId, json, attr, false);
}

export function createProxyOptionsBuffer(modifiedBody, req) {
  // if request transformer returned modified body,
  // serialize it to new request stream. Original
  // request stream was already consumed. See `buffer` option
  // in node-http-proxy.
  if (typeof modifiedBody === 'object') {
    modifiedBody = JSON.stringify(modifiedBody);
  }
  if (typeof modifiedBody === 'string') {
    req.headers['content-length'] = modifiedBody.length;
    // create an array
    modifiedBody = [modifiedBody];
  } else {
    logger.error(`Unknown type: '${modifiedBody}'`);
    throw 'Unknown type';
  }
  return streamify(modifiedBody);
}

export function removeTenantPrefix(tenantId, json, jsonPath, allowGlobal) {
  const tenantWithUnderscore = withUnderscore(tenantId);
  const globalPrefix = withUnderscore(GLOBAL_PREFIX);
  const result = findValuesByJsonPath(json, jsonPath);
  for (const idx in result) {
    const item = result[idx];
    const prop = item.parent[item.parentProperty];
    if (allowGlobal && prop.indexOf(globalPrefix) == 0) {
      continue;
    }
    // expect tenantId prefix
    if (prop.indexOf(tenantWithUnderscore) != 0) {
      logger.error(
        `Name must start with tenantId prefix` +
          `tenantId:'${tenantId}',json:'${json}',jsonPath:'${jsonPath}'` +
          `,item:'${item}'`,
      );
      throw 'Name must start with tenantId prefix'; // TODO create Exception class
    }
    // remove prefix
    item.parent[item.parentProperty] = prop.substr(tenantWithUnderscore.length);
  }
}

export function removeTenantPrefixes(tenantId, json, jsonPathToAllowGlobal) {
  for (const key in jsonPathToAllowGlobal) {
    removeTenantPrefix(tenantId, json, key, jsonPathToAllowGlobal[key]);
  }
}

export function findValuesByJsonPath(json, path, resultType = 'all') {
  const result = JSONPath({json, path, resultType});
  logger.debug(`For path '${path}' found ${result.length} items`);
  return result;
}
