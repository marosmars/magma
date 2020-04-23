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

// Currently just filters result without passing prefix to conductor.
// TODO: implement querying by prefix in conductor
import {
  addTenantIdPrefix,
  assertAllowedSystemTask,
  createProxyOptionsBuffer,
  withInfixSeparator,
} from '../utils.js';

const logger = logging.getLogger(module);

// Gets all task definition
/*
curl  -H "x-auth-organization: fb-test" "localhost/proxy/api/metadata/taskdefs"
*/
function getAllTaskdefsAfter(tenantId, req, respObj) {
  // iterate over taskdefs, keep only those belonging to tenantId or global
  // remove tenantId prefix, keep GLOBAL_
  const tenantWithInfixSeparator = withInfixSeparator(tenantId);
  for (let idx = respObj.length - 1; idx >= 0; idx--) {
    const taskdef = respObj[idx];
    if (taskdef.name.indexOf(tenantWithInfixSeparator) == 0) {
      taskdef.name = taskdef.name.substr(tenantWithInfixSeparator.length);
    } else {
      // remove element
      respObj.splice(idx, 1);
    }
  }
}

// Used in POST and PUT
function sanitizeTaskdefBefore(tenantId, taskdef) {
  // only whitelisted system tasks are allowed
  assertAllowedSystemTask(taskdef);
  // prepend tenantId
  addTenantIdPrefix(tenantId, taskdef);
}
// Create new task definition(s)
// Underscore in name is not allowed.
/*
curl -X POST -H "x-auth-organization: fb-test"  \
 "localhost/proxy/api/metadata/taskdefs" \
 -H 'Content-Type: application/json' -d '
[
    {
      "name": "bar",
      "retryCount": 3,
      "retryLogic": "FIXED",
      "retryDelaySeconds": 10,
      "timeoutSeconds": 300,
      "timeoutPolicy": "TIME_OUT_WF",
      "responseTimeoutSeconds": 180,
      "ownerEmail": "foo@bar.baz"
    }
]
'
*/
// TODO: should this be disabled?
function postTaskdefsBefore(tenantId, req, res, proxyCallback) {
  // iterate over taskdefs, prefix with tenantId
  const reqObj = req.body;
  for (let idx = 0; idx < reqObj.length; idx++) {
    const taskdef = reqObj[idx];
    sanitizeTaskdefBefore(tenantId, taskdef);
  }
  proxyCallback({buffer: createProxyOptionsBuffer(reqObj, req)});
}

// Update an existing task
// Underscore in name is not allowed.
/*
curl -X PUT -H "x-auth-organization: fb-test" \
 "localhost/proxy/api/metadata/taskdefs" \
 -H 'Content-Type: application/json' -d '
    {
      "name": "frinx",
      "retryCount": 3,
      "retryLogic": "FIXED",
      "retryDelaySeconds": 10,
      "timeoutSeconds": 400,
      "timeoutPolicy": "TIME_OUT_WF",
      "responseTimeoutSeconds": 180,
      "ownerEmail": "foo@bar.baz"
    }
'
*/
// TODO: should this be disabled?
function putTaskdefBefore(tenantId, req, res, proxyCallback) {
  const reqObj = req.body;
  const taskdef = reqObj;
  sanitizeTaskdefBefore(tenantId, taskdef);
  proxyCallback({buffer: createProxyOptionsBuffer(reqObj, req)});
}

/*
curl -H "x-auth-organization: fb-test" \
 "localhost/proxy/api/metadata/taskdefs/frinx"
*/
// Gets the task definition
function getTaskdefByNameBefore(tenantId, req, res, proxyCallback) {
  req.params.name = withInfixSeparator(tenantId) + req.params.name;
  // modify url
  req.url = '/api/metadata/taskdefs/' + req.params.name;
  proxyCallback();
}

function getTaskdefByNameAfter(tenantId, req, respObj, res) {
  if (res.status == 200) {
    const tenantWithInfixSeparator = withInfixSeparator(tenantId);
    // remove prefix
    if (respObj.name && respObj.name.indexOf(tenantWithInfixSeparator) == 0) {
      respObj.name = respObj.name.substr(tenantWithInfixSeparator.length);
    } else {
      logger.error(
        `Tenant Id prefix '${tenantId}' not found, taskdef name: '${respObj.name}'`,
      );
      res.status(400);
      res.send('Prefix not found'); // TODO: this exits the process
    }
  }
}

// TODO: can this be disabled?
// Remove a task definition
/*
curl -H "x-auth-organization: fb-test" \
 "localhost/api/metadata/taskdefs/bar" -X DELETE -v
*/
function deleteTaskdefByNameBefore(tenantId, req, res, proxyCallback) {
  req.params.name = withInfixSeparator(tenantId) + req.params.name;
  // modify url
  req.url = '/api/metadata/taskdefs/' + req.params.name;
  proxyCallback();
}

export default function() {
  return [
    {
      method: 'get',
      url: '/api/metadata/taskdefs',
      after: getAllTaskdefsAfter,
    },
    {
      method: 'post',
      url: '/api/metadata/taskdefs',
      before: postTaskdefsBefore,
    },
    {
      method: 'put',
      url: '/api/metadata/taskdefs',
      before: putTaskdefBefore,
    },
    {
      method: 'get',
      url: '/api/metadata/taskdefs/:name',
      before: getTaskdefByNameBefore,
      after: getTaskdefByNameAfter,
    },
    {
      method: 'delete',
      url: '/api/metadata/taskdefs/:name',
      before: deleteTaskdefByNameBefore,
    },
  ];
}
