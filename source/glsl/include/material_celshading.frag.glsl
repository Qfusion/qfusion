myhalf3 CelShading(in myhalf3 surfaceNormalModelspace, in myhalf3 diffuseNormalModelspace)
{ 
	myhalf diffuseProductPositive;
	myhalf diffuseProductNegative;
	myhalf diffuseProduct;
	myhalf hardShadow = 0.0;

#ifdef APPLY_HALFLAMBERT
	diffuseProduct = myhalf (dot (surfaceNormalModelspace, diffuseNormalModelspace));
	diffuseProductPositive = myhalf ( clamp(diffuseProduct, 0.0, 1.0) * 0.5 + 0.5 );
	diffuseProductPositive *= diffuseProductPositive;
	diffuseProductNegative = myhalf ( clamp(diffuseProduct, -1.0, 0.0) * 0.5 - 0.5 );
	diffuseProductNegative = diffuseProductNegative * diffuseProductNegative - 0.25;
	diffuseProduct = diffuseProductPositive;
#else
	diffuseProduct = myhalf (dot (surfaceNormalModelspace, diffuseNormalModelspace));
	diffuseProductPositive = max (diffuseProduct, 0.0);
	diffuseProductNegative = (-min (diffuseProduct, 0.0) - 0.3);
#endif // APPLY_HALFLAMBERT

	// smooth the hard shadow edge
	hardShadow += floor(max(diffuseProduct + 0.1, 0.0) * 2.0);
	hardShadow += floor(max(diffuseProduct + 0.055, 0.0) * 2.0);
	hardShadow += floor(diffuseProductPositive * 2.0);

	diffuseProduct = myhalf(0.6 + hardShadow * 0.09 + diffuseProductPositive * 0.14);

	// backlight
	diffuseProduct += myhalf (ceil(diffuseProductNegative * 2.0) * 0.085 + diffuseProductNegative * 0.085);

	return myhalf3(diffuseProduct);
}
