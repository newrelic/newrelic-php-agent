//
// Copyright 2020 New Relic Corporation. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//

package collector

import (
	"bytes"
	"compress/zlib"
	"encoding/base64"
	"io/ioutil"
)

func Compress(b []byte) (*bytes.Buffer, error) {
	buf := bytes.Buffer{}
	w := zlib.NewWriter(&buf)
	_, err := w.Write(b)
	w.Close()

	if nil != err {
		return nil, err
	}

	return &buf, nil
}

func Uncompress(b []byte) ([]byte, error) {
	buf := bytes.NewBuffer(b)
	r, err := zlib.NewReader(buf)
	if nil != err {
		return nil, err
	}
	defer r.Close()

	return ioutil.ReadAll(r)
}

func CompressEncode(b []byte) (string, error) {
	compressed, err := Compress(b)

	if nil != err {
		return "", err
	}
	return base64.StdEncoding.EncodeToString(compressed.Bytes()), nil
}

func UncompressDecode(s string) ([]byte, error) {
	decoded, err := base64.StdEncoding.DecodeString(s)
	if nil != err {
		return nil, err
	}

	return Uncompress(decoded)
}
