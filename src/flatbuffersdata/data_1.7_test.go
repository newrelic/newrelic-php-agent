//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

// +build !go1.8

package flatbuffersdata

const testFlatbuffersTxnDataExpectedJSON = `[` +
	`{"name":"Supportability/TxnData/CustomEvents","forced":true,"data":[1,3,0,3,3,9]},` +
	`{"name":"Supportability/TxnData/Metrics","forced":true,"data":[1,2,0,2,2,4]},` +
	`{"name":"Supportability/TxnData/Size","forced":true,"data":[1,1296,0,1296,1296,1.679616e+06]},` +
	`{"name":"Supportability/TxnData/SlowSQL","forced":true,"data":[1,1,0,1,1,1]},` +
	`{"name":"Supportability/TxnData/TraceSize","forced":true,"data":[1,2,0,2,2,4]},` +
	`{"name":"forced","forced":true,"data":[6,5,4,3,2,1]},` +
	`{"name":"scoped","forced":false,"data":[1,2,3,4,5,6]},` +
	`{"name":"scoped","forced":false,"data":[1,2,3,4,5,6]}` +
	`]`
