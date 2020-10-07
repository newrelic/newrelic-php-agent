<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_record_custom_event() with one event with a gigantic number of
parameters.
*/

/*INI
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
[
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type":"testType",
        "timestamp":"??"
      },
      {
        "key63": "value63",
        "key62": "value62",
        "key61": "value61",
        "key60": "value60",
        "key59": "value59",
        "key58": "value58",
        "key57": "value57",
        "key56": "value56",
        "key55": "value55",
        "key54": "value54",
        "key53": "value53",
        "key52": "value52",
        "key51": "value51",
        "key50": "value50",
        "key49": "value49",
        "key48": "value48",
        "key47": "value47",
        "key46": "value46",
        "key45": "value45",
        "key44": "value44",
        "key43": "value43",
        "key42": "value42",
        "key41": "value41",
        "key40": "value40",
        "key39": "value39",
        "key38": "value38",
        "key37": "value37",
        "key36": "value36",
        "key35": "value35",
        "key34": "value34",
        "key33": "value33",
        "key32": "value32",
        "key31": "value31",
        "key30": "value30",
        "key29": "value29",
        "key28": "value28",
        "key27": "value27",
        "key26": "value26",
        "key25": "value25",
        "key24": "value24",
        "key23": "value23",
        "key22": "value22",
        "key21": "value21",
        "key20": "value20",
        "key19": "value19",
        "key18": "value18",
        "key17": "value17",
        "key16": "value16",
        "key15": "value15",
        "key14": "value14",
        "key13": "value13",
        "key12": "value12",
        "key11": "value11",
        "key10": "value10",
        "key9": "value9",
        "key8": "value8",
        "key7": "value7",
        "key6": "value6",
        "key5": "value5",
        "key4": "value4",
        "key3": "value3",
        "key2": "value2",
        "key1": "value1",
        "key0": "value0"
      },
      {}
    ]
  ]
]
*/

for ($i = 0; $i < 1000; $i++) {
  $parameters["key" . $i] = "value" . $i;
}

newrelic_record_custom_event("testType", $parameters);
