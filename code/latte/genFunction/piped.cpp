/* ----------------------------------------------------------------- */
/*                                                                   */
/* LattE (Lattice Point Enumeration)                                 */
/*                                                                   */
/* Computing all lattice points in fundamental parallelepiped        */
/*                                                                   */
/* Author     : Raymond Hemmecke, Ruriko Yoshida                     */
/*                                                                   */
/* Created    : 07-JUN-02                                            */
/* Last Update: 20-DEC-02 by Rudy                                    */
/*                                                                   */
/* ----------------------------------------------------------------- */
#include "config.h"
#include "../myheader.h"
#include "../cone.h"
#include "../print.h"
#include "../ramon.h"
#include "../rational.h"
#include "../vertices/cdd.h"
#include <cassert>
#include <string>

#include "IntCombEnum.h"
#include "matrix_ops.h" 
#include <NTL/mat_ZZ.h>
#include "convert.h"
#include "piped.h"

using namespace std;

/*
 * Description:
 * pointsInParallelepiped: 1) Find Smith Normal form of cone basis. In other
 * words, find a basis v and w such that v_i = n_i*w_i. 2) Enumerate all
 * lattice points in fund||piped by taking all (bounded: 0 <= k <= n_{i-1})
 * integer combinations of w_i, and translating them accordingly
 *
 * Parameters:
 * listCone *cone : data pertaining to the cone
 * int numOfVars: dimension of cone
 *
 * Return: listVector* of lattice points
 */


/*
 * calculates a integer combination of the specified lattice basis
 * and stores the result in the vector v. Because of incompatible datatypes,
 * it is easiest to simply do this directly.
 */
static vec_ZZ
get_integer_comb(const mat_ZZ & lat_basis, int *scalar)
{
   int i,j;
   ZZ conv_l;
   int row = lat_basis.NumRows();
   int col = lat_basis.NumCols();
   vec_ZZ v = createVector(col);

   for (i = 0; i < row; i++) {
      v[i] = 0;
      for (j = 0; j < col; j++) {
         v[i] += lat_basis[i][j] * scalar[j]; 
      }
   }
   return (v);
}

void
PointsInParallelepipedGenerator::compute_multiplier(ZZ &multiplier, const vec_ZZ &m,
						    const vec_ZZ &facet, int facet_index)
{
  InnerProduct(multiplier, m, facet);
  multiplier = beta[facet_index] - multiplier;
  multiplier %= cone->facet_divisors[facet_index];
  multiplier -= beta[facet_index];
}

/*
 * Translate point m using the following formula:
 * m' = v + sum {<m - v, u_i*>}u_i, where {} means the fractional part 
 */
#if 1
vec_ZZ
PointsInParallelepipedGenerator::translate_lattice_point(const vec_ZZ& m)
{
  vec_ZZ result;
  int dim = beta.length();
  result.SetLength(dim);
  int i;
  listVector *facet;
  listVector *ray;
  ZZ multiplier;
  for (i = 0, facet = cone->facets, ray = cone->rays;
       i<dim;
       i++, facet=facet->rest, ray=ray->rest) {
    compute_multiplier(multiplier, m, facet->first, i);
    result += (multiplier * facet_scale_factors[i]) * ray->first;
  }
  for (i = 0; i<dim; i++) {
    ZZ q, r;
    DivRem(q, r, result[i], facet_divisor_common_multiple);
    assert(IsZero(r));
    result[i] = q;
  }
  return result;
}
#else

static mat_ZZ
get_U_star(const listCone *cone)
{
   /* Note that < RAY_i, FACET_j > = -FACET_DIVISOR_i * DELTA_{i,j}. */
   mat_ZZ U_star = convert_listVector_to_mat_ZZ(cone->facets);

   return (U_star);
}

vec_ZZ
PointsInParallelepipedGenerator::translate_lattice_point(const vec_ZZ& m)
{
  mat_ZZ U_star = get_U_star(cone);
   int cols = U.NumCols();
   int len = m.length();
   mat_ZZ U_trans = transpose(U);
   rationalVector *v = cone->vertex;
   /* initialized to zero */
   ZZ dot_prod_enum, dot_prod_denom;
   ZZ frac_enum, frac_denom, q, r, neg;
   vec_ZZ scaled_u_i, m_trans;
   vec_ZZ offset_enum, sum_enum, m_trans_enum;
   vec_ZZ offset_denom, sum_denom, m_trans_denom;

   /* allocate memory for intermediate calculation vectors */
   offset_enum.SetLength(len);
   offset_denom.SetLength(len);
   sum_enum.SetLength(len);
   sum_denom.SetLength(len);
   m_trans.SetLength(len);

   /* initialize sum */
   sum_enum = v->enumerator;
   sum_denom = v->denominator;

   /* w = m - v */
   for (int j = 0; j < len; j++) {
      /* m - x/y = (my - x)/y */
      offset_enum[j] = (m[j] * v->denominator[j]) - v->enumerator[j];
      offset_denom[j] = v->denominator[j];
   }

   for (int i = 0; i < cols; i++) {
      /* reset/initialize variables */
      dot_prod_enum = to_ZZ(0);
      dot_prod_denom = to_ZZ(1);

      /* < m - v, u_i* > */
      for (int j = 0; j < len; j++) {
         /* neg = - U_star[j][i] */
         NTL::negate(neg, U_star[j][i]);
         /* x/y + r/s*z = x/y + rz/s = xs + rzy/ys */
         dot_prod_enum = (dot_prod_enum * offset_denom[j]) +
            (dot_prod_denom * offset_enum[j] * neg);
         dot_prod_denom *= (offset_denom[j]);
      }
      dot_prod_denom *= cone->facet_divisors[i];
   
      /*
       * {<m - v, u_i*>} : take the fractional part of this quantity
       * q = floor(dot_prod_nume/dot_prod_denom)
       * r = dot_prod_enum - dot_prod_denom*q
       */
      DivRem(q, r, dot_prod_enum, dot_prod_denom);
      frac_enum = r;
      frac_denom = dot_prod_denom;

      /* {<m - v, u_i*>}u_i */
      scaled_u_i = U_trans[i] * frac_enum;

      /* sum {<m - v, u_i*>}u_i */
      for (int j = 0; j < len; j++) {
         /* w/z + x/y = (wy + xz)/zy */
         sum_enum[j] = (sum_enum[j] * frac_denom) +
            (sum_denom[j] * scaled_u_i[j]);
         sum_denom[j] *= frac_denom;
      }
   }

   for (int j = 0; j < len; j++) {
      /*
       * This MUST be an integer point!! This MUST divide evenly!
       * m_trans = floor(sum_enum/sum_denom)
       * r = sum_enum - sum_denom*q
       */
      DivRem(m_trans[j], r, sum_enum[j], sum_denom[j]);
      assert(IsZero(r));
   }

   return (m_trans);
}
#endif


/*
 * If S is the Smith Normal Form of A, then the multipliers n_i such that
 * v_i = n_i w_i are on the diagonal
 */
static vector<int>
get_multipliers_from_snf(const mat_ZZ & snf)
{
   int j;
   int row = snf.NumRows();
   vector<int> n(row);

   for (j = 0; j < row; j++) {
      //cout << "get_multipliers_from_snf:: snf[" << j << "," << j << "] = " << snf[j][j] << "\n"; 
     if (snf[j][j] > INT_MAX) {
       cerr << "Implementation restriction hit:  Smith normal form has entries larger than INT_MAX\n";
       abort();
     }
      n[j] = to_int(snf[j][j]);
   }
   return n;
}

PointsInParallelepipedGenerator::PointsInParallelepipedGenerator(const listCone *a_cone, int numOfVars) :
  cone(a_cone)
{
  U = convert_listVector_to_mat_ZZ(cone->rays);

  if (abs(cone->determinant) == 1) {
    /* Unimodular case: Id = Smith(U) = B U C.
       Thus, for instance, B = det U * U^{-1}, C = Id.
     */
    max_multipliers = vector<int>(numOfVars);
    int i;
    for (i = 0; i<numOfVars; i++)
      max_multipliers[i] = 1;
    B_inv = cone->determinant * U;
  }
  else
    {
    /* General case: */
    mat_ZZ snf_U, B, C;

    //cout << "Computing Smith-Normal form...\n";
    /* get Smith Normal form of matrix, Smith(U) = BUC */
    snf_U = SmithNormalForm(U, B, C);

#ifdef CHECK_SNF
    {
      mat_ZZ SU = B * U * C;
      assert(SU == snf_U);
      int i, j;
      for (i = 0; i<numOfVars; i++) {
	for (j = 0; j<numOfVars; j++)
	  if (i != j) assert(IsZero(snf_U[i][j]));
      }
      assert(abs(determinant(B)) == 1);
      assert(abs(determinant(C)) == 1);
    }
#endif
    /* extract n_i such that v_i = n_i*w_i from Smith Normal form */
    max_multipliers = get_multipliers_from_snf(snf_U);

    /*
     * Smith(U) = BUC, and the diagonal entries of Smith(U) are the multipliers
     * n_i such that w_i*n_i= v_i. Therefore, UC = V, and B^-1 = W. Since we
     * must take the integer combinations of W, we must calculate B^-1.
     */
#if 0
    /* However, this is expensive. --mkoeppe */
    B_inv = inv(B);
#else
    /* We also have B^-1 = UC Smith(U)^{-1}; this is faster to
       compute because Smith(U) is diagonal. --mkoeppe */
    B_inv = U * C;
    int i, j;
    ZZ q, r;
    for (i = 0; i<B_inv.NumRows(); i++) {
      for (j = 0; j<B_inv.NumCols(); j++) {
	DivRem(q, r, B_inv[i][j], snf_U[j][j]);
	assert(IsZero(r));
	B_inv[i][j] = q;
      }
    }
#endif
  }
  /* We compute beta_i = floor(<v, facet_i>) modulo facet_divisor_i. */
  {
    ZZ v_scale_factor;
    vec_ZZ v_scaled = scaleRationalVectorToInteger(cone->vertex,
						   numOfVars, v_scale_factor);
    beta.SetLength(numOfVars);
    int i;
    listVector *facet;
    ZZ sp;
    for (i = 0, facet = cone->facets; i<numOfVars; i++, facet=facet->rest) {
      InnerProduct(sp, v_scaled, facet->first);
      div(beta[i], sp, v_scale_factor);
      assert(beta[i] * v_scale_factor <= sp);
    }
  }
  facet_scale_factors.SetLength(numOfVars);
  facet_divisor_common_multiple = abs(cone->determinant);
  {
    int i;
    for (i = 0; i<numOfVars; i++)
      facet_scale_factors[i] = facet_divisor_common_multiple / cone->facet_divisors[i];
  }
}

const vector<int> &
PointsInParallelepipedGenerator::GetMaxMultipliers()
{
  return max_multipliers;
}

vec_ZZ
PointsInParallelepipedGenerator::GeneratePoint(int *multipliers)
{
  vec_ZZ lat_pt = get_integer_comb(B_inv, multipliers);
  return translate_lattice_point(lat_pt);
}

static void
check_point_in_parallelepiped(listCone *cone, const vec_ZZ &point)
{
  int numOfVars = point.length();
  ZZ v_scale_factor;
  vec_ZZ v_scaled = scaleRationalVectorToInteger(cone->vertex,
						 numOfVars, v_scale_factor);
  listVector *facet, *ray;
  for (facet = cone->facets, ray = cone->rays;
       facet;
       facet = facet->rest, ray = ray->rest) {
    vec_ZZ &f = facet->first;
    vec_ZZ &r = ray->first;
    ZZ psp;
    InnerProduct(psp, f, point);
    psp *= v_scale_factor;
    ZZ vsp;
    InnerProduct(vsp, f, v_scaled);
    ZZ rsp;
    InnerProduct(rsp, f, r);
    rsp *= v_scale_factor;
    assert(psp <= vsp);
    assert(psp > vsp + rsp);
  }
}

static bool
are_points_unique(listVector *points)
{
  listVector *a, *b;
  for (a = points; a; a = a->rest) {
    for (b = a->rest; b; b = b->rest) {
      if (a->first == b->first) return false;
    }
  }
  return true;
}

listVector*
pointsInParallelepiped(listCone *cone, int numOfVars)
{
  PointsInParallelepipedGenerator generator(cone, numOfVars);
  vector<int> max_multipliers = generator.GetMaxMultipliers();
  /*
   * enumerate lattice points by taking all integer combinations
   * 0 <= k <= (n_i - 1) of each vector.
   */
  int length = max_multipliers.size();
  int *n = new int[length];
  int i;
  for (i = 0; i<length; i++)
    n[i] = max_multipliers[i];
  IntCombEnum iter_comb(n, length);
  //cout << "Enumerating lattice points...\n";
  iter_comb.decrementUpperBound();
  listVector *lat_points = NULL;
  int *next;
  while((next = iter_comb.getNext())) {
    vec_ZZ trans_lat_pt = generator.GeneratePoint(next);
#ifdef CHECK_ENUM
    check_point_in_parallelepiped(cone, trans_lat_pt);
#endif CHECK_ENUM
    lat_points = appendVectorToListVector(trans_lat_pt, lat_points);
  }
#ifdef CHECK_ENUM
  assert(lengthListVector(lat_points) == abs(cone->determinant));
  if (!are_points_unique(lat_points)) {
    printListCone(cone, numOfVars);
    cerr << "Smith: ";
    for (i = 0; i<length; i++) {
      cerr << max_multipliers[i] << "; ";
    }
    for (i = 0; i<length; i++) {
      cerr << n[i] << " ";
    }
    cerr << endl;
    printListVector(lat_points, numOfVars);
    // Enumerate again
    for (i = 0; i<length; i++)
      n[i] = max_multipliers[i];
    IntCombEnum iter_comb(n, length);
    //cout << "Enumerating lattice points...\n";
    iter_comb.decrementUpperBound();
    int *next;
    while((next = iter_comb.getNext())) {
      vec_ZZ lat_pt = get_integer_comb(generator.B_inv, next);
      for (i = 0; i<length; i++) {
	cerr << lat_pt[i] << " ";
      }
      cerr << "--> ";
      vec_ZZ trans_lat_pt = generator.translate_lattice_point(lat_pt);
      check_point_in_parallelepiped(cone, trans_lat_pt);
      for (i = 0; i<length; i++) {
	cerr << trans_lat_pt[i] << " ";
      }
      cerr << endl;
    }
    abort();
  };
#endif
  delete[] n;
  return (lat_points);
}

/* ----------------------------------------------------------------- */
listVector* pointsInParallelepipedOfUnimodularCone(rationalVector *vertex, 
						  listVector *rays, 
						  int numOfVars) {
  int i,j,k,numOfRays;
  ZZ a,b;
  vec_ZZ lambda,w,z;
  vec_ZZ *matrix, *originalMatrix;
  listVector *points, *tmp;
  rationalVector *coeffs;

/*  printf("Computing point in parallelepiped of unimodular cone.\n"); */

/*  printRationalVector(vertex,numOfVars); */

  //points=createListVector(createVector(numOfVars));

  numOfRays=lengthListVector(rays);
  if (numOfRays!=numOfVars) {
    printf("Cone is NOT simplicial!\n");
    exit(1);
  }

/*  printf("numOfRays = %d, numOfVars = %d\n", numOfRays, numOfVars); */

  matrix=createArrayVector(numOfVars);
  originalMatrix=createArrayVector(numOfVars);

  for (i=0; i<numOfVars; i++) matrix[i]=createVector(numOfRays);

  k=0;
  tmp=rays;
  while (tmp) {
    for (i=0; i<numOfVars; i++) matrix[i][k]=(tmp->first)[i];
    k++;
    tmp=tmp->rest;
  }

 for (i=0; i<numOfRays; i++) 
    originalMatrix[i]=copyVector(matrix[i],numOfRays);

  for (i=0; i<numOfVars; i++) {
    for (j=0; j<numOfRays; j++) {
      matrix[i][j]=matrix[i][j]*(vertex->denominator[i]);
    }
  }
  w=copyVector(vertex->enumerator,numOfVars);

  /* Now we have to solve: matrix * x = w */

  coeffs=solveLinearSystem(matrix,w,numOfVars,numOfRays);

  lambda=createVector(numOfRays);

/* Note that we make heavy use of numOfVars=numOfRays! That is, we
   assume the cone to be simplicial! */

  for (i=0; i<numOfVars; i++) {
    a=coeffs->denominator[i];
    b=coeffs->enumerator[i];

    if (((b/a)*a)==b) {
      lambda[i]=b/a;
    } else {
      if (((b<0) && (a>0)) || ((b>0) && (a<0))) {
//          lambda[i]=-abs(b)/abs(a); 
         lambda[i]=abs(b)/abs(a); 
         lambda[i]=-lambda[i]; 
     } else 
	lambda[i]=b/a;
      if (b>0) lambda[i]=lambda[i]+1;
    }
  }

//  cout << "lambda = ";
//  printVector(lambda,numOfVars);

  z=createVector(numOfVars);
  for (i=0; i<numOfVars; i++) z[i]=0;

  for (i=0; i<numOfRays; i++) {
    for (j=0; j<numOfVars; j++) {
      z[j]=z[j]+lambda[i]*originalMatrix[j][i];
    }
  }

//  cout << "point = ";
//  printVector(z,numOfVars);

  //points->rest=createListVector(z);
  
  coeffs->denominator.kill ();
  coeffs->enumerator.kill ();

  delete coeffs;
  
  points=createListVector(z);
  delete [] matrix;
  delete [] originalMatrix;
  w.kill();
  z.kill();
  lambda.kill();

  return(points);
}
/* ----------------------------------------------------------------- */

void computePointsInParallelepiped(listCone *cone, int numOfVars)
{
#if 1
#if 0
  if (abs(cone->determinant) != 1)
    cout << "Processing cone with determinant " << cone->determinant << endl;
#endif
  cone->latticePoints = pointsInParallelepiped(cone, numOfVars);
#else  
  if (abs(cone->determinant) != 1) {
     cout << "Processing cone with determinant " << cone->determinant << endl;
      cone->latticePoints = pointsInParallelepiped(cone, numOfVars);
   } else {
      cone->latticePoints = pointsInParallelepipedOfUnimodularCone(
         cone->vertex, cone->rays, numOfVars);
   }
#endif
}

void computePointsInParallelepipeds(listCone *cones, int numOfVars)
{
  listCone *tmp = cones;
  int Cones_Processed_Count = 0;
  while (tmp) {
    computePointsInParallelepiped(tmp, numOfVars);
    tmp=tmp->rest;
    Cones_Processed_Count++;
    if ((Cones_Processed_Count % 1000) == 0 )
      cout << Cones_Processed_Count << " cones processed." << endl;
  }
}

/* ----------------------------------------------------------------- */

PointsScalarProductsGenerator::PointsScalarProductsGenerator
(const listCone *a_cone, int numOfVars, const vec_ZZ &a_generic_vector) :
  PointsInParallelepipedGenerator(a_cone, numOfVars),
  generic_vector(a_generic_vector)
{
  scaled_ray_scalar_products.SetLength(numOfVars);
  int i;
  ZZ inner;
  listVector *ray;
  for (i = 0, ray = cone->rays; i<numOfVars; i++, ray=ray->rest) {
    InnerProduct(inner, generic_vector, ray->first);
    scaled_ray_scalar_products[i] = facet_scale_factors[i] * inner;
  }
}

ZZ PointsScalarProductsGenerator::GeneratePointScalarProduct(int *multipliers)
{
  vec_ZZ lat_pt = get_integer_comb(B_inv, multipliers);
#if 0
  vec_ZZ trans_pt = translate_lattice_point(lat_pt);
  ZZ result;
  InnerProduct(result, generic_vector, trans_pt);
  return result;
#else
  /* Equivalent, but faster code. */
  ZZ result;
  result = 0;
  int dim = beta.length();
  int i;
  listVector *facet;
  listVector *ray;
  ZZ multiplier;
  for (i = 0, facet = cone->facets, ray = cone->rays;
       i<dim;
       i++, facet=facet->rest, ray=ray->rest) {
    compute_multiplier(multiplier, lat_pt, facet->first, i);
    result += multiplier * scaled_ray_scalar_products[i];
  }
  ZZ q, r;
  DivRem(q, r, result, facet_divisor_common_multiple);
  assert(IsZero(r));
  return q;
#endif  
}

void computeLatticePointsScalarProducts(listCone *cone, int numOfVars,
					const vec_ZZ &generic_vector)
{
  ZZ index = abs(cone->determinant);
  if (index > INT_MAX) {
    cerr << "Implementation restriction hit:  Attempt to enumerate a fundamental parallelepiped of index greater than INT_MAX.  (Probably not a good idea anyway.)" << endl;
    abort();
  }
  int num_lattice_points = to_long(index);
  cone->lattice_points_scalar_products.SetLength(num_lattice_points);

  if (cone->latticePoints) {
    // Lattice points already computed.
    listVector *point;
    int i;
    for (point = cone->latticePoints, i = 0; point != NULL; point = point->rest, i++) {
      InnerProduct(cone->lattice_points_scalar_products[i],
		   generic_vector, point->first);
    }
  }
  else {
    PointsScalarProductsGenerator generator(cone, numOfVars, generic_vector);
    vector<int> max_multipliers = generator.GetMaxMultipliers();
    /*
     * enumerate lattice points by taking all integer combinations
     * 0 <= k <= (n_i - 1) of each vector.
     */
    int length = max_multipliers.size();
    int *n = new int[length];
    int i;
    for (i = 0; i<length; i++)
      n[i] = max_multipliers[i];
    IntCombEnum iter_comb(n, length);
    //cout << "Enumerating lattice points...\n";
    iter_comb.decrementUpperBound();
    listVector *lat_points = NULL;
    int *next;
    int num_scalar = 0;
    while((next = iter_comb.getNext())) {
      cone->lattice_points_scalar_products[num_scalar]
	= generator.GeneratePointScalarProduct(next);
      num_scalar++;
    }
    delete[] n;
  }
}
