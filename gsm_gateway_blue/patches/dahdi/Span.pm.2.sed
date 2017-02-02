	$self->{IS_ALLOGSM} = 0;

	foreach my $cardtype (@alloGsm_strings) {
		if($head =~ m/$cardtype/) {
			my $termtype = "gsm";
			$self->{IS_DIGITAL} = 1;
			$self->{IS_ALLOGSM} = 1;
			$self->{TERMTYPE} = $termtype; # TE, NT
			$self->{TYPE} = "BRI_$termtype";
			$self->{DCHAN_IDX} = 1;
			$self->{BCHAN_LIST} = [0];
			last;
		}
	}
