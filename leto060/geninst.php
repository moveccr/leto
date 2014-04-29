<?php
//
// Leto060 geninst.php
// Copyright (C) 2014 isaki@NetBSD.org 
//
// 鎌田さんところの M68K 命令表
// http://www.retropc.net/x68000/software/develop/as/has060/m68k.htm
// の該当部分だけをコピペして取り出したテキストファイルから
// instructions.txt を生成するヘルパースクリプト。
//

	if ($argc < 2) {
		print "Usage: {$argv[0]} <m68k.html> > instructions.txt\n";
		exit(1);
	}

	$fp = fopen($argv[1], "r");
	if ($fp === false) {
		print "fopen {$argv[1]} failed\n";
		exit(1);
	}

	while (($line = fgets($fp))) {
		$line = trim(preg_replace("/;.*/", "", $line));
		if ($line == "") {
			continue;
		}

		// 1行はこんな感じ
		// CMP2.bwl <ea>,Rn            --234S|-|-UUUU|-U*U*|  M  WXZP |00000ss011mmmrrr-rnnn000000000000
		// 列は '|'(パイプ) で区切られている。最初の列だけ例外。
		// これを "$name $mpu|$priv|$ccr_in|$ccr_out|$addr|$bits_all" に分解。
		// $name … ニーモニック
		// $mpu  … MPU ごとのサポート有無
		// $priv … 特権命令なら 'P'
		// $ccr_in, $ccr_out … CCR の変化
		// $addr … 対応しているアドレッシングモード
		// $bits_all … ビットパターン。1ワードずつのリスト
		// 書式と凡例については元ページ参照

		$item = preg_split("/\|/", $line, -1, PREG_SPLIT_NO_EMPTY);
		$name_mpu = array_shift($item);
		list ($name, $mpu) = preg_split("/  +/", $name_mpu,
			-1, PREG_SPLIT_NO_EMPTY);
		list ($priv, $ccr_in, $ccr_out, $addr, $bits_all) = $item;

		// ビットパターンの後ろにコメントがあれば取り除く
		$m = preg_split("/ +/", $bits_all, 2, PREG_SPLIT_NO_EMPTY);
		$bits_all = $m[0];

		// ビットパターンをワードごとに分解。
		$bits = preg_split("/-/", $bits_all);

		// ここでは1ワード目しか使わない
		$bits = $bits[0];

		// MOVE命令の "xxxxxx" を "RRRMMM" に置換。読みやすさのため
		$bits = preg_replace("/xxxxxx/", "RRRMMM", $bits);
		// サイズは "s" と "ss" があるので "ss" のほうを "SS" に
		$bits = preg_replace("/ss/", "SS", $bits);

		// カラムを並び替えて出力
		print "{$bits}|{$mpu}|{$addr}|\t|{$name}\n";
	}
?>
